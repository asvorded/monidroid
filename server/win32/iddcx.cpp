#include "native.h"

#include <windows.h>
#include <swdevice.h>
#include <wrl.h>
#include <d3d11_3.h>
#include <dxgi1_5.h>

#include "iddcx.h"
#include "monidroid/logger.h"

using namespace Microsoft::WRL;

static constexpr int MAX_RETRIES = 5;
static constexpr int RETRY_INTERVAL = 500;

static constexpr auto HEALTH_CHECK_TAG = L"IddCx health check";

static constexpr auto ADAPTER_TAG = "Monidroid Adapter";

static constexpr auto MONITOR_TAG = "<unnamed monitor>";

struct AdapterContext {
    AdapterContext(HANDLE handle);
    ~AdapterContext();

public:
    HANDLE handle;
};

AdapterContext::AdapterContext(HANDLE handle) : handle(handle) {}

AdapterContext::~AdapterContext() {
    CloseHandle(handle);
}

struct MonitorContext {
    ~MonitorContext();

public:
    std::weak_ptr<AdapterContext> adapter;

    std::string modelName;
    MonitorMode currentMode;

    UINT connectorIndex;
    LUID adapterLuid;
    HANDLE driverProcess = NULL;
    HANDLE thisProcess = GetCurrentProcess();

    ComPtr<ID3D11Device3> m_device;
    ComPtr<ID3D11DeviceContext3> m_deviceContext;

    ComPtr<ID3D11Texture2D> currentFrame;
};

MonitorContext::~MonitorContext() {
    if (driverProcess != NULL) CloseHandle(driverProcess);
    if (thisProcess != NULL) CloseHandle(thisProcess);
}

void MonitorContextDeleter::operator()(MonitorContext* p) const {
    delete p;
}

static void WINAPI CreationCallback(
    _In_ HSWDEVICE hSwDevice,
    _In_ HRESULT hrCreateResult,
    _In_ PVOID pContext,
    _In_ PCWSTR pszDeviceInstanceId
) {
    HANDLE hEvent = *(HANDLE*)pContext;
    SetEvent(hEvent);

    UNREFERENCED_PARAMETER(hSwDevice);
    UNREFERENCED_PARAMETER(hrCreateResult);
}

static HRESULT CreateVirtualAdapter() {
    HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (hEvent == NULL) {
        DWORD err = GetLastError();
        Monidroid::TaggedLog(HEALTH_CHECK_TAG, L"Failed to create adapter: CreateEvent() failed with {}", err);
        return HRESULT_FROM_WIN32(err);
    }

    SW_DEVICE_CREATE_INFO createInfo = {
        .cbSize = sizeof(createInfo),
        // These match the Pnp id's in the inf file so
        // OS will load the driver when the device is created
        .pszInstanceId = L"0001",
        .pszzHardwareIds = L"MonidroidDriver\0\0",
        .pszzCompatibleIds = L"MonidroidDriver\0\0",
        .CapabilityFlags = SWDeviceCapabilitiesRemovable |
            SWDeviceCapabilitiesSilentInstall |
            SWDeviceCapabilitiesDriverRequired,
        .pszDeviceDescription = L"Monidroid SWD graphics adapter",
    };

    // Create the device
    HSWDEVICE hSwDevice;
    HRESULT hr = SwDeviceCreate(L"MonidroidDriver", L"HTREE\\ROOT\\0",
        &createInfo, 0, nullptr, CreationCallback, &hEvent, &hSwDevice);
    if (FAILED(hr)) {
        return hr;
    }

    // Wait for callback to signal that the device has been created
    Monidroid::TaggedLog(HEALTH_CHECK_TAG, L"Waiting for device to be created....\n");
    DWORD waitResult = WaitForSingleObject(hEvent, 5 * 1000);
    if (waitResult != WAIT_OBJECT_0) {
        Monidroid::TaggedLog(HEALTH_CHECK_TAG, L"Wait for device creation failed\n");
        return E_FAIL;
    }
    Monidroid::TaggedLog(HEALTH_CHECK_TAG, L"Device created\n\n");
    return S_OK;
}

HRESULT CreateD3DDevice(LUID adapterLuid, ID3D11Device3** device, ID3D11DeviceContext3** deviceContext) {
    ComPtr<IDXGIFactory5> pFactory;
    ComPtr<IDXGIAdapter> pAdapter;
    ComPtr<ID3D11Device> pDevice;
    ComPtr<ID3D11DeviceContext> pDeviceContext;

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { return hr; }

    D3D_DRIVER_TYPE type;
    if (adapterLuid.HighPart == 0 && adapterLuid.LowPart == 0) {
        type = D3D_DRIVER_TYPE_HARDWARE;
    } else {
        hr = pFactory->EnumAdapterByLuid(adapterLuid, IID_PPV_ARGS(&pAdapter));
        if (FAILED(hr)) { return hr; }
        type = D3D_DRIVER_TYPE_UNKNOWN;
    }

    hr = D3D11CreateDevice(
        pAdapter.Get(), type, NULL,
#ifdef DEBUG
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
#else
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
#endif
        FEATURE_LEVELS, FEATURE_LEVELS_COUNT, D3D11_SDK_VERSION, &pDevice, NULL, &pDeviceContext
    );
    if (FAILED(hr)) { return hr; }

    pDevice->QueryInterface(IID_PPV_ARGS(device));
    pDeviceContext->QueryInterface(IID_PPV_ARGS(deviceContext));

    return hr;
}

void videoHealthCheck() noexcept(false) {
	bool manuallyCreated = false;

    for (int retries = 0; retries < MAX_RETRIES; ++retries) {
		HANDLE hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
			GENERIC_READ | GENERIC_WRITE, 0,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
		);
        if (hAdapter != INVALID_HANDLE_VALUE) {
            Monidroid::TaggedLog(HEALTH_CHECK_TAG, L"Monidroid Graphics Adapter detected");
            CloseHandle(hAdapter);
            return;
        }

		if (!manuallyCreated) {
            HRESULT hr = CreateVirtualAdapter();
            if (FAILED(hr)) {
                throw std::system_error(hr, std::system_category(), "Unable to continue because Monidroid Graphics Adapter cannot be created!");
            }
            manuallyCreated = true;
        } else {
            Sleep(RETRY_INTERVAL);
        }
	}

    throw std::runtime_error("Unable to continue because Monidroid Graphics Adapter is inaccessible!");
}

Adapter openAdapter() {
    HANDLE hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
        GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hAdapter == INVALID_HANDLE_VALUE) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Failed to open adapter, error code: {}", GetLastError());
        return Adapter();
    }

    return Adapter(new AdapterContext(hAdapter));
}

Monitor adapterConnectMonitor(const Adapter& self, const std::string& modelName, const MonitorMode& info) {
    ADAPTER_MONITOR_INFO monitorInfo {
        .monitorNumberBySocket = INVALID_SOCKET,
        .width = info.width,
        .height = info.height,
        .hertz = info.refreshRate,
    };
    ADAPTER_MONITOR_INFO monitorInfoOut = {};

    DWORD bytesReceived = 0;

    if (!DeviceIoControl(self->handle, IOCTL_IDDCX_MONITOR_CONNECT,
        &monitorInfo, sizeof(monitorInfo), &monitorInfoOut, sizeof(monitorInfoOut),
        &bytesReceived, NULL)
    ) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Failed to connect monitor, error code: {}", GetLastError());
        return Monitor();
    }

    HANDLE hDriverProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, monitorInfoOut.driverProcessId);
    if (hDriverProcess == NULL) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Failed to initialize monitor, OpenProcess() returned {}", GetLastError());
        return Monitor();
    }

    Monitor monitor = Monitor(new MonitorContext {
        .adapter = self,
        .modelName = modelName.empty() ? MONITOR_TAG : modelName,
        .currentMode = info,
        .connectorIndex = monitorInfoOut.connectorIndex,
        .adapterLuid = monitorInfoOut.adapterLuid,
        .driverProcess = hDriverProcess,
    });

    HRESULT hr = CreateD3DDevice(monitor->adapterLuid, &monitor->m_device, &monitor->m_deviceContext);
    if (FAILED(hr)) {
        Monidroid::TaggedLog(monitor->modelName, "D3D device pre-creation failed, HRESULT: {:#010X}, ignoring", (unsigned long)hr);
        monitor->adapterLuid = { };
    }

    return monitor;
}

MDStatus monitorRequestFrame(const Monitor& monitor) {
    Adapter adapter = monitor->adapter.lock();
    if (!adapter) {
        throw std::runtime_error("Unexpected inaccessibility of Monidroid Graphics Adapter!");
    }

    FRAME_MONITOR_INFO in { .connectorIndex = monitor->connectorIndex };
    FRAME_MONITOR_INFO out { };
    DWORD bytesReceived = -1;

    if (!DeviceIoControl(adapter->handle, IOCTL_IDDCX_REQUEST_FRAME,
        &in, sizeof(in), &out, sizeof(out),
        &bytesReceived, nullptr)
    ) {
        Monidroid::TaggedLog(monitor->modelName, "Frame request failed, DeviceIoControl() returned {}", GetLastError());
        return MDStatus::Error;
    }

    // Check monitor power state
    if (!out.enabled) {
        return MDStatus::MonitorOff;
    }

    // Check frame handle (if there are any updates)
    if (!out.frameHandle) {
        return MDStatus::NoUpdates;
    }
    
    // Duplicate driver's render adapter to open shared resources
    if (out.adapterLuid != monitor->adapterLuid) {
        Monidroid::TaggedLog(monitor->modelName, "Adapter LUID mismatch had been detected, D3D device will be recreated");
        monitor->m_device.Reset();
        monitor->m_deviceContext.Reset();
        HRESULT hr = CreateD3DDevice(out.adapterLuid, &monitor->m_device, &monitor->m_deviceContext);
        if (FAILED(hr)) {
            Monidroid::TaggedLog(monitor->modelName, "Failed to initialize D3D device, HRESULT: {:#010X}", (unsigned long)hr);
            return MDStatus::Error;
        }
        monitor->adapterLuid = out.adapterLuid;
    }

    HANDLE thisFrameHandle = NULL;

    if (!DuplicateHandle(
        monitor->driverProcess, out.frameHandle,
        monitor->thisProcess, &thisFrameHandle,
        0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS
    )) {
        Monidroid::TaggedLog(monitor->modelName, "Failed to open frame resource, DuplicateHandle() returned {}", GetLastError());
        CloseHandle(out.frameHandle);
        return MDStatus::Error;
    }

    ComPtr<ID3D11Texture2D> sharedTexture;
    HRESULT hr = monitor->m_device->OpenSharedResource1(thisFrameHandle, IID_PPV_ARGS(&sharedTexture));
    CloseHandle(thisFrameHandle);
    if (FAILED(hr)) {
        Monidroid::TaggedLog(monitor->modelName, "D3D shared resource cannot be opened, HRESULT: {:#010X}", (unsigned long)hr);
        return MDStatus::Error;
    }

    D3D11_TEXTURE2D_DESC desc;
    sharedTexture->GetDesc(&desc);

    // Create staging
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    
    monitor->currentFrame.Reset();
    hr = monitor->m_device->CreateTexture2D(&desc, nullptr, &monitor->currentFrame);
    if (FAILED(hr)) {
        Monidroid::TaggedLog(monitor->modelName, "Failed to stage frame, HRESULT: {:#010X}", (unsigned long)hr);
        return MDStatus::Error;
    }

    monitor->m_deviceContext->CopyResource(monitor->currentFrame.Get(), sharedTexture.Get());

    // TODO: maybe handle refresh rate?
    if (monitor->currentMode.width != desc.Width && monitor->currentMode.height != desc.Height) {
        Monidroid::TaggedLog(monitor->modelName, "Mode change: {}x{} -> {}x{}",
            monitor->currentMode.width, monitor->currentMode.height,
            desc.Width, desc.Height
        );
        monitor->currentMode.width = desc.Width;
        monitor->currentMode.height = desc.Height;
        return MDStatus::ModeChanged;
    }
    return MDStatus::FrameReady;
}

MonitorMode monitorRequestMode(const Monitor& self, bool cached) {
    // TODO: make non-cached
    return self->currentMode;
}

void monitorMapCurrent(const Monitor& self, FrameMapInfo& mapInfo) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = self->m_deviceContext->Map(self->currentFrame.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desc;
        self->currentFrame->GetDesc(&desc);
        mapInfo = {
            .data = static_cast<ColorType*>(mapped.pData),
            .width = desc.Width,
            .height = desc.Height,
            .stride = mapped.RowPitch
        };
    } else {
        Monidroid::TaggedLog(self->modelName, "Frame mapping failed, HRESULT: {:#010X}", (unsigned long)hr);
        mapInfo = { };
    }
}

void monitorUnmap(const Monitor& self) {
    self->m_deviceContext->Unmap(self->currentFrame.Get(), 0);
}

void monitorDisconnect(Monitor& monitor) {
    if (auto adapter = monitor->adapter.lock()) {
        ADAPTER_MONITOR_INFO monitorInfo = {};
        monitorInfo.monitorNumberBySocket = INVALID_SOCKET; // TODO
        monitorInfo.connectorIndex = monitor->connectorIndex;
        ADAPTER_MONITOR_INFO monitorInfoOut = {};

        DWORD bytesReceived = 0;

        if (!DeviceIoControl(adapter->handle, IOCTL_IDDCX_MONITOR_DISCONNECT,
            &monitorInfo, sizeof(monitorInfo), &monitorInfoOut, sizeof(monitorInfoOut), &bytesReceived, NULL)) {
            Monidroid::TaggedLog(ADAPTER_TAG, "Failed to disconnect \"{}\", error code: {}", monitor->modelName, GetLastError());
        }
    } else {
        Monidroid::TaggedLog(monitor->modelName, "Adapter is inaccessible, ignoring disconnect request...");
    }
    monitor.reset();
}
