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

    HRESULT Init();

public:
    std::weak_ptr<AdapterContext> adapter;

    std::string modelName;
    MonitorMode currentMode;

    UINT connectorIndex;
    LUID adapterLuid;
    HANDLE driverProcess = NULL;
    HANDLE thisProcess = NULL;

    ComPtr<ID3D11Device3> m_device;
    ComPtr<ID3D11DeviceContext3> m_deviceContext;
};

MonitorContext::~MonitorContext() {
    if (driverProcess != NULL) CloseHandle(driverProcess);
    if (thisProcess != NULL) CloseHandle(thisProcess);
}

HRESULT MonitorContext::Init() {
    thisProcess = GetCurrentProcess();

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
        FEATURE_LEVELS, FEATURE_LEVELS_COUNT, D3D11_SDK_VERSION, &pDevice, NULL, &pDeviceContext
    );
    if (FAILED(hr)) { return hr; }

    pDevice->QueryInterface(IID_PPV_ARGS(&m_device));
    pDeviceContext->QueryInterface(IID_PPV_ARGS(&m_deviceContext));

    return hr;
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

void videoHealthCheck() noexcept(false) {
	bool manuallyCreated = false;

    for (int retries = 0; retries < MAX_RETRIES; ++retries) {
		HANDLE hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
			GENERIC_READ | GENERIC_WRITE, 0,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
		);
#ifdef DEBUG
        Monidroid::TaggedLog(HEALTH_CHECK_TAG, L"CreateFile() code: {}", GetLastError());
#endif
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

Monitor adapterConnectMonitor(const Adapter& adapter, const std::string& modelName, const MonitorMode& info) {
    ADAPTER_MONITOR_INFO monitorInfo {
        .monitorNumberBySocket = INVALID_SOCKET,
        .width = info.width,
        .height = info.height,
        .hertz = info.refreshRate,
    };
    ADAPTER_MONITOR_INFO monitorInfoOut = {};

    DWORD bytesReceived = 0;

    if (!DeviceIoControl(adapter->handle, IOCTL_IDDCX_MONITOR_CONNECT,
        &monitorInfo, sizeof(monitorInfo), &monitorInfoOut, sizeof(monitorInfoOut),
        &bytesReceived, NULL)
    ) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Failed to connect monitor, error code: {}", GetLastError());
        return Monitor();
    }

    HANDLE hDriverProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, monitorInfoOut.driverProcessId);
    if (hDriverProcess == NULL) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Failed to initialize monitor (open driver process failed), error code: {}", GetLastError());
        return Monitor();
    }

    Monitor monitor = Monitor(new MonitorContext {
        .adapter = adapter,
        .modelName = modelName.empty() ? MONITOR_TAG : modelName,
        .currentMode = info,
        .connectorIndex = monitorInfoOut.connectorIndex,
        .adapterLuid = monitorInfoOut.adapterLuid,
        .driverProcess = hDriverProcess,
    });

    HRESULT hr = monitor->Init();
    if (FAILED(hr)) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Failed to initialize monitor, error code: {}", GetLastError());
        return Monitor();
    }

    return monitor;
}

MDStatus monitorRequestFrame(const Monitor& monitor, std::unique_ptr<ColorType[]>& buffer, unsigned int& bufferPixSize) {
    Adapter adapter = monitor->adapter.lock();
    if (!adapter) {
        throw std::runtime_error("Unexpected inaccessibility of Monidroid Graphics Adapter!");
    }

    FRAME_MONITOR_INFO frameInfo { .connectorIndex = monitor->connectorIndex };
    FRAME_MONITOR_INFO frameInfoOut { };
    DWORD bytesReceived = -1;

    if (!DeviceIoControl(adapter->handle, IOCTL_IDDCX_REQUEST_FRAME,
        &frameInfo, sizeof(frameInfo), &frameInfoOut, sizeof(frameInfoOut),
        &bytesReceived, nullptr)
    ) {
        Monidroid::TaggedLog(monitor->modelName, "Failed to open frame resource, DeviceIoControl() error code: {}", GetLastError());
        return MDStatus::NotAvailable; // TODO
    }

    HANDLE thisFrameHandle;

    if (!DuplicateHandle(
        monitor->driverProcess, frameInfoOut.frameHandle,
        monitor->thisProcess, &thisFrameHandle,
        0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS
    )) {
        Monidroid::TaggedLog(monitor->modelName, "Failed to open frame resource, DuplicateHandle() error code: {}", GetLastError());
        CloseHandle(frameInfoOut.frameHandle);
        return MDStatus::Error;
    }

    ComPtr<ID3D11Texture2D> sharedTexture;
    HRESULT hr = monitor->m_device->OpenSharedResource1(thisFrameHandle, IID_PPV_ARGS(&sharedTexture));
    if (FAILED(hr)) {
        Monidroid::TaggedLog(monitor->modelName, "Failed to open frame resource, HRESULT: {:#010X}", hr);
        return MDStatus::Error;
    }

    CloseHandle(thisFrameHandle);

    D3D11_TEXTURE2D_DESC desc;
    sharedTexture->GetDesc(&desc);
    if (desc.Width != monitor->currentMode.width || desc.Height != monitor->currentMode.height) {
        Monidroid::TaggedLog(monitor->modelName, "Frame request is not performed due to mode change");
        return MDStatus::ModeChanged;
    }

    if (auto pixSize = desc.Width * desc.Height; pixSize > bufferPixSize) {
        buffer = std::make_unique<ColorType[]>(pixSize);
        bufferPixSize = pixSize;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = monitor->m_deviceContext->Map(sharedTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        Monidroid::TaggedLog(monitor->modelName, "Failed to copy frame, Map() HRESULT: {:#010X}", hr);
        return MDStatus::Error;
    }

    for (int y = 0; y < desc.Height; ++y) {
        std::memcpy(
            buffer.get() + y * desc.Width,
            (uint8_t*)mapped.pData + y * mapped.RowPitch,
            desc.Width * sizeof(*buffer.get())
        );
    }

    return MDStatus::Ok;
}

void monitorRequestMode(const Monitor& monitor, MonitorMode& infoOut, bool cached) {
    // TODO: make non-cached
    infoOut = monitor->currentMode;
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
