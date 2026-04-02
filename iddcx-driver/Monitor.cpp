#include "Monitor.h"

#include <avrt.h>

MonitorContext::MonitorContext(IDDCX_MONITOR Monitor)
  : m_monitor(Monitor),
    m_preffered()
{ }

MonitorContext::~MonitorContext() { }

void MonitorContext::SetupMonitor(const ADAPTER_MONITOR_INFO* pMonitirInfo) {
    m_preffered.width = pMonitirInfo->width;
    m_preffered.height = pMonitirInfo->height;
    m_preffered.refreshRate = pMonitirInfo->hertz;
}

const MonitorMode& MonitorContext::PreferredMode() const {
    return m_preffered;
}

HRESULT MonitorContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID AdapterLuid, HANDLE hNextSurfaceAvailable) {
    m_pProcessor.reset(new MonitorProcessor(SwapChain, hNextSurfaceAvailable));
    HRESULT hr = m_pProcessor->Init(AdapterLuid);
    if (SUCCEEDED(hr)) {
        hr = m_pProcessor->Start();
    } else {
        m_pProcessor.reset();
    }
    return hr;
}

void MonitorContext::UnassignSwapChain() {
    m_pProcessor->Stop();
    m_pProcessor.reset();
}

HRESULT MonitorContext::RequestFrame(FRAME_MONITOR_INFO* pInfo) {
    if (m_pProcessor) {
        return m_pProcessor->RequestFrame(*pInfo);
    } else {
        return E_NOT_VALID_STATE;
    }
}

MonitorProcessor::MonitorProcessor(IDDCX_SWAPCHAIN swapChain, HANDLE hNextSurfaceAvailable)
  : m_swapChain(swapChain),
    m_newFrameEvent(hNextSurfaceAvailable),
    m_stopEvent(),
    m_frameReadyEvent(),
    m_thread(),
    m_frameRequested(false),
    m_currentFrameInfo()
{ }

HRESULT MonitorProcessor::Init(LUID adapterLuid) {
    ComPtr<IDXGIFactory5> pFactory;
    ComPtr<IDXGIAdapter> pAdapter;
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { return hr; }

    hr = pFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&pAdapter));
    if (FAILED(hr)) { return hr; }

    hr = D3D11CreateDevice(
        pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        FEATURE_LEVELS, FEATURE_LEVELS_COUNT, D3D11_SDK_VERSION, &pDevice, nullptr, &pDeviceContext
    );
    if (FAILED(hr)) { return hr; }

    hr = pDevice.As(&m_pDevice);
    if (FAILED(hr)) { return hr; }

    hr = pDeviceContext.As(&m_pDeviceContext);
    if (FAILED(hr)) { return hr; }

    return S_OK;
}

HRESULT MonitorProcessor::Start() {
    m_stopEvent = CreateEventW(nullptr, false, false, nullptr);
    if (m_stopEvent == NULL) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    m_frameReadyEvent = CreateEventW(nullptr, false, false, nullptr);
    if (m_frameReadyEvent == NULL) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    m_thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    if (m_thread == NULL) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

void MonitorProcessor::Stop() {
    SetEvent(m_stopEvent);

    DWORD result = WaitForSingleObject(m_thread, MAX_STOP_WAIT);
    if (result == WAIT_TIMEOUT) {
        throw std::system_error(ERROR_TIMEOUT, std::system_category(), "Processor stop failure");
    }
}

MonitorProcessor::~MonitorProcessor() {
    CloseHandle(m_newFrameEvent);
    
    if (m_swapChain != nullptr)    WdfObjectDelete(m_swapChain);

    if (m_thread != NULL)          CloseHandle(m_thread);
    if (m_stopEvent != NULL)       CloseHandle(m_stopEvent);
    if (m_frameReadyEvent != NULL) CloseHandle(m_frameReadyEvent);
}

DWORD MonitorProcessor::ThreadProc(void* arg) {
    DWORD AvTask = 0;
    HANDLE AvTaskHandle = AvSetMmThreadCharacteristicsW(L"Distribution", &AvTask);

    auto* self = static_cast<MonitorProcessor*>(arg);
    self->StartPrivate();
    
    WdfObjectDelete(self->m_swapChain);
    self->m_swapChain = nullptr;

    AvRevertMmThreadCharacteristics(AvTaskHandle);
    return 0;
}

HRESULT MonitorProcessor::StartPrivate() {
    ComPtr<IDXGIDevice3> pSwapChainDevice;
    HRESULT hr = m_pDevice.As(&pSwapChainDevice);
    if (FAILED(hr)) return hr;

    IDARG_IN_SWAPCHAINSETDEVICE chainSetDevice { .pDevice = pSwapChainDevice.Get() };
    hr = IddCxSwapChainSetDevice(m_swapChain, &chainSetDevice);
    // WTF is going on when it returns 0x887A0026 (DXGI_ERROR_ACCESS_LOST)?????
    if (FAILED(hr)) {
        return hr;
    }

    while (true) {
        BufferArgs args = {};
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_swapChain, &args);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING) {
            // We must wait for a new buffer
            HANDLE WaitHandles[] {
                m_newFrameEvent,
                m_stopEvent
            };
            DWORD WaitResult = WaitForMultipleObjects(ARRAYSIZE(WaitHandles), WaitHandles, FALSE, 16);
            if (WaitResult == WAIT_OBJECT_0 || WaitResult == WAIT_TIMEOUT) {
                // We have a new buffer
                continue;
            } else if (WaitResult == WAIT_OBJECT_0 + 1) {
                // We need to terminate
                hr = S_OK;
                break;
            } else {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
        } else if (SUCCEEDED(hr)) {
            if (m_frameRequested) {
                CopyFrame(args);
                
                InterlockedExchange(&m_frameRequested, false);
                SetEvent(m_frameReadyEvent);
            }
            args.MetaData.pSurface->Release();

            hr = IddCxSwapChainFinishedProcessingFrame(m_swapChain);
            if (FAILED(hr)) { break; }
        } else {
            // Some error happened
            // Next step is reassigning swap-chain (??)
            break;
        }
    }

    return hr;
}

HRESULT MonitorProcessor::RequestFrame(FRAME_MONITOR_INFO& info) {
    InterlockedExchange(&m_frameRequested, true);

    HANDLE waits[] {
        m_frameReadyEvent,
        m_stopEvent,
    };
    DWORD result = WaitForMultipleObjects(ARRAYSIZE(waits), waits, false, MAX_FRAME_WAIT);
    switch (result) {
    case WAIT_OBJECT_0:
        // Frame ready
        break;
    case WAIT_OBJECT_0 + 1:
        // Sudden processor stop detected
        return E_ABORT;
        break;
    case WAIT_TIMEOUT:
        return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        break;
    default:
        return E_UNEXPECTED;
    }

    info.frameHandle = m_currentFrameInfo.frameHandle;
    info.timeStamp = m_currentFrameInfo.timeStamp;

    return S_OK;
}

HRESULT MonitorProcessor::CopyFrame(const BufferArgs& args) {
    IDXGIResource* source = args.MetaData.pSurface;

    ComPtr<ID3D11Texture2D> pTexture;
    source->QueryInterface(IID_PPV_ARGS(&pTexture));

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> pRequestedTexture;
    HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pRequestedTexture);
    if (SUCCEEDED(hr)) {
        m_pDeviceContext->CopyResource(pTexture.Get(), pRequestedTexture.Get());

        ComPtr<IDXGIResource1> pRequestedFrame;
        pRequestedTexture->QueryInterface(IID_PPV_ARGS(&pRequestedFrame));

        hr = pRequestedFrame->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &m_currentFrameInfo.frameHandle);
        if (SUCCEEDED(hr)) {
            m_currentFrameInfo.timeStamp = args.MetaData.PresentDisplayQPCTime;
        }
    }
    return S_OK;
}

