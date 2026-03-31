#include "Monitor.h"

MonitorContext::MonitorContext(IDDCX_MONITOR Monitor)
    : m_monitor(Monitor)
    , m_preffered()
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
    m_pProcessor = std::make_unique<MonitorProcessor>(SwapChain, AdapterLuid, hNextSurfaceAvailable);
    HRESULT hr = m_pProcessor->Start();
    if (FAILED(hr)) {
        m_pProcessor->Stop();
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

MonitorProcessor::MonitorProcessor(IDDCX_SWAPCHAIN swapChain, LUID adapterLuid, HANDLE hNextSurfaceAvailable)
    : m_swapChain(swapChain),
    m_hNewFrameEvent(hNextSurfaceAvailable),
    m_hStopEvent(NULL) {
    ComPtr<IDXGIFactory5> pFactory;
    ComPtr<IDXGIAdapter> pAdapter;
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { }

    hr = pFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&pAdapter));
    if (FAILED(hr)) { }

    hr = D3D11CreateDevice(
        pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        FEATURE_LEVELS, FEATURE_LEVELS_COUNT, D3D11_SDK_VERSION, &pDevice, nullptr, &pDeviceContext
    );
    if (FAILED(hr)) { }

    hr = pDevice->QueryInterface(IID_PPV_ARGS(&m_pDevice));
    if (FAILED(hr)) { }

    hr = pDeviceContext->QueryInterface(IID_PPV_ARGS(&m_pDeviceContext));
    if (FAILED(hr)) { }
}

MonitorProcessor::~MonitorProcessor() {
    WdfObjectDelete(m_swapChain);
    CloseHandle(m_hNewFrameEvent);
    CloseHandle(m_hStopEvent);
}

HRESULT MonitorProcessor::Start() {
    m_hStopEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_hStopEvent == NULL) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    IDXGIDevice3* pSwapChainDevice;
    HRESULT hr = m_pDevice->QueryInterface(IID_PPV_ARGS(&pSwapChainDevice));
    if (FAILED(hr)) return hr;

    IDARG_IN_SWAPCHAINSETDEVICE chainSetDevice { .pDevice = pSwapChainDevice };
    hr = IddCxSwapChainSetDevice(m_swapChain, &chainSetDevice);
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT MonitorProcessor::RequestFrame(FRAME_MONITOR_INFO& info) {
    IDARG_OUT_RELEASEANDACQUIREBUFFER bufferArgs;
    HRESULT hr = IddCxSwapChainReleaseAndAcquireBuffer(m_swapChain, &bufferArgs);
    if (hr == E_PENDING) {
        HANDLE waitHandles[] = {
            m_hStopEvent,
            m_hNewFrameEvent,
        };

        DWORD waitCode = WaitForMultipleObjects(ARRAYSIZE(waitHandles), waitHandles, FALSE, INFINITE);
        if (waitCode == WAIT_OBJECT_0) {
            // We must stop capturing
            IddCxSwapChainFinishedProcessingFrame(m_swapChain);
            return E_ABORT;
        }
        // We have a new buffer
        hr = IddCxSwapChainReleaseAndAcquireBuffer(m_swapChain, &bufferArgs);
        if (FAILED(hr)) return hr;
    } else if (FAILED(hr)) return hr;

    ComPtr<IDXGIResource> pSurface(bufferArgs.MetaData.pSurface);
    ComPtr<ID3D11Texture2D> pTexture;
    pSurface->QueryInterface(IID_PPV_ARGS(&pTexture));

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> pRequestedTexture;
    hr = m_pDevice->CreateTexture2D(&desc, nullptr, &pRequestedTexture);
    if (SUCCEEDED(hr)) {
        m_pDeviceContext->CopyResource(pTexture.Get(), pRequestedTexture.Get());

        ComPtr<IDXGIResource1> pRequestedFrame;
        pRequestedTexture->QueryInterface(IID_PPV_ARGS(&pRequestedFrame));

        hr = pRequestedFrame->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &info.frameHandle);
        if (SUCCEEDED(hr)) {
            info.timeStamp = bufferArgs.MetaData.PresentDisplayQPCTime;
        }
    }

    return FAILED(hr) ? hr : IddCxSwapChainFinishedProcessingFrame(m_swapChain);
}

void MonitorProcessor::Stop() {
    SetEvent(m_hStopEvent);
}