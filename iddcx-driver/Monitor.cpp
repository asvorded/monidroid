#include "Monitor.h"

#include <avrt.h>
#include <sddl.h>

MonitorContext::MonitorContext(IDDCX_MONITOR Monitor)
  : m_monitor(Monitor),
    m_preffered(),
    m_current(),
    // Monitor must be enabled after "commit modes" callback
    m_enabled(false)
{ }

MonitorContext::~MonitorContext() { }

void MonitorContext::SetupMonitor(const ADAPTER_MONITOR_INFO* pMonitirInfo) {
    m_preffered = {
        .width = pMonitirInfo->width,
        .height = pMonitirInfo->height,
        .refreshRate = pMonitirInfo->hertz,
    };
}

const MonitorMode& MonitorContext::PreferredMode() const {
    return m_preffered;
}

void MonitorContext::CommitMode(const DISPLAYCONFIG_VIDEO_SIGNAL_INFO& signalInfo, const IDDCX_PATH_FLAGS flags) {
    m_enabled = (flags & IDDCX_PATH_FLAGS_ACTIVE);
    if (!m_enabled) {
        return;
    }

#ifdef DBG
    if (signalInfo.vSyncFreq.Denominator != 1) {
        // vSyncFreq.Denominator must be 1 according to MakeSignalInfo()
        throw std::runtime_error("Unexpected signal info with vSyncFreq.Denominator not equal 1");
    }
#endif
    m_current = {
        .width = signalInfo.activeSize.cx,
        .height = signalInfo.activeSize.cy,
        .refreshRate = signalInfo.vSyncFreq.Numerator / signalInfo.AdditionalSignalInfo.vSyncFreqDivider,
    };
}


HRESULT MonitorContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID AdapterLuid, HANDLE hNextSurfaceAvailable) {
    if (m_pProcessor) {
        // Swap-chain is being reassigned, stop current processor
        m_pProcessor->Stop();
    }

    m_pProcessor.reset(new MonitorProcessor(SwapChain, AdapterLuid, hNextSurfaceAvailable));
    HRESULT hr = m_pProcessor->Init();
    if (SUCCEEDED(hr)) {
        hr = m_pProcessor->Start();
        if (SUCCEEDED(hr)) {
            return hr;
        }
    }

    // There is failure here
    m_pProcessor.reset();
    return hr;
}

void MonitorContext::UnassignSwapChain() {
    m_pProcessor->Stop();
    m_pProcessor.reset();
}

HRESULT MonitorContext::RequestFrame(FRAME_MONITOR_INFO& info) {
    // Initialized in server, not here
    //info.adapterLuid = { };
    //info.frameHandle = { };
    //info.metadata = { };

    if (m_enabled && m_pProcessor) {
        HRESULT hr = m_pProcessor->RequestFrame(info);
        if (SUCCEEDED(hr)) {
            info.metadata.mode = m_current;
            info.enabled = true;
        } else {
            info.enabled = false;
        }
        return hr == E_ABORT ? S_OK : hr;
    } else {
        // Monitor is off (if processor is empty ??)
        info.enabled = false;
        return S_OK;
    }
}

MonitorProcessor::MonitorProcessor(IDDCX_SWAPCHAIN swapChain, LUID AdapterLuid, HANDLE hNextSurfaceAvailable)
  : m_swapChain(swapChain),
    m_newFrameEvent(hNextSurfaceAvailable),
    m_stopEvent(NULL),
    m_frameReadyEvent(NULL),
    m_thread(NULL),
    m_frameRequested(false),
    m_currentMetadata(),
    m_adapterLuid(AdapterLuid)
{ }

HRESULT MonitorProcessor::Init() {
    ComPtr<IDXGIFactory5> pFactory;
    ComPtr<IDXGIAdapter> pAdapter;
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { return hr; }

    hr = pFactory->EnumAdapterByLuid(m_adapterLuid, IID_PPV_ARGS(&pAdapter));
    if (FAILED(hr)) { return hr; }

    hr = D3D11CreateDevice(
        pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL,
#ifdef DBG
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
#else
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
#endif
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
    // Stop event has manual reset
    m_stopEvent = CreateEventW(nullptr, true, false, nullptr);
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

#ifdef DBG
    DWORD result = WaitForSingleObject(m_thread, MAX_STOP_WAIT);
    if (result == WAIT_TIMEOUT) {
        throw std::system_error(ERROR_TIMEOUT, std::system_category(), "Processor stop failure");
    }
#else
    DWORD result = WaitForSingleObject(m_thread, INFINITE);
#endif
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
    
    // Set event to avoid MAX_FRAME_WAIT waiting
    // Probable calls order:
    // RequestFrame()      <- here we don't know state of processor thread
    // ------------------
    // Assign/UnassignSwapChain()
    // ------------------  <- here old processor is 100% stopped and freed
    // RequestFrame()
    SetEvent(self->m_stopEvent);

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
            // Next step is reassigning swap-chain or processor stop
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
    case WAIT_OBJECT_0: {
        // Frame ready
        ComPtr<IDXGIResource1> res;
        if (!m_currentFrame) {
            // maybe return another code?
            return E_OUTOFMEMORY;
        }
        m_currentFrame->QueryInterface(IID_PPV_ARGS(&res));

        HANDLE handle = NULL;
        HRESULT hr = res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &handle);
        if (SUCCEEDED(hr)) {
            info.metadata = m_currentMetadata;
            info.frameHandle = handle;
            info.adapterLuid = m_adapterLuid;
        }

        return hr;
    }
    case WAIT_OBJECT_0 + 1: {
        // Processor suddenly stopped
        // maybe: 1) mode change, 2) internal error
        //info.metadata.enabled = false;
        return E_ABORT;
    }
    case WAIT_TIMEOUT: {
        // No pending desktop updates
        //info.metadata.enabled = true;
        return S_OK;
    }
    }

    // Unexpected failure
    return E_UNEXPECTED;
}

HRESULT MonitorProcessor::CopyFrame(const BufferArgs& args) {
    IDXGIResource* source = args.MetaData.pSurface;

    ComPtr<ID3D11Texture2D> pSourceTexture;
    source->QueryInterface(IID_PPV_ARGS(&pSourceTexture));

    D3D11_TEXTURE2D_DESC desc;
    pSourceTexture->GetDesc(&desc);
    // Cannot create STAGING and SHARED_NTHANDLE texture
    // IddCx texture has SHARED_NTHANDLE misc flag by default

    HRESULT hr = m_pDevice->CreateTexture2D(&desc, nullptr, &m_currentFrame);
    if (SUCCEEDED(hr)) {
        m_pDeviceContext->CopyResource(m_currentFrame.Get(), pSourceTexture.Get());
        m_currentMetadata = {
            .timeStamp = args.MetaData.PresentDisplayQPCTime,
            .frameNumber = args.MetaData.PresentationFrameNumber,
        };
    }
    return S_OK;
}

