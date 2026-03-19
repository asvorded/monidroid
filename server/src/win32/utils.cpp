#include "utils.h"

#include <windows.h>
#include <swdevice.h>

#include <monidroid/iddcx.h>
#include <monidroid/logger.h>

static constexpr int MAX_RETRIES = 5;

static constexpr auto TAG = L"IddCx health check";

VOID WINAPI
CreationCallback(
    _In_ HSWDEVICE hSwDevice,
    _In_ HRESULT hrCreateResult,
    _In_opt_ PVOID pContext,
    _In_ PCWSTR pszDeviceInstanceId
) {
    HANDLE hEvent = *(HANDLE*)pContext;
    SetEvent(hEvent);

    UNREFERENCED_PARAMETER(hSwDevice);
    UNREFERENCED_PARAMETER(hrCreateResult);
}

HRESULT CreateVirtualAdapter() {
    HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    HSWDEVICE hSwDevice;
    SW_DEVICE_CREATE_INFO createInfo = { 0 };
    PCWSTR description = L"Monidroid IddCx SWD adapter";

    // These match the Pnp id's in the inf file so OS will load the driver when the device is created
    PCWSTR instanceId = L"0001";
    PCWSTR hardwareIds = L"MonidroidDriver\0\0";
    PCWSTR compatibleIds = L"MonidroidDriver\0\0";

    createInfo.cbSize = sizeof(createInfo);
    createInfo.pszzCompatibleIds = compatibleIds;
    createInfo.pszInstanceId = instanceId;
    createInfo.pszzHardwareIds = hardwareIds;
    createInfo.pszDeviceDescription = description;

    createInfo.CapabilityFlags = SWDeviceCapabilitiesRemovable |
        SWDeviceCapabilitiesSilentInstall |
        SWDeviceCapabilitiesDriverRequired;

    // Create the device
    HRESULT hr = SwDeviceCreate(L"MonidroidDriver", L"HTREE\\ROOT\\0",
        &createInfo, 0, nullptr, CreationCallback, &hEvent, &hSwDevice);
    if (FAILED(hr)) {
        return hr;
    }

    // Wait for callback to signal that the device has been created
    Monidroid::TaggedLog(TAG, L"Waiting for device to be created....\n");
    DWORD waitResult = WaitForSingleObject(hEvent, 5 * 1000);
    if (waitResult != WAIT_OBJECT_0) {
        Monidroid::TaggedLog(TAG, L"Wait for device creation failed\n");
        return E_FAIL;
    }
    Monidroid::TaggedLog(TAG, L"Device created\n\n");
    return S_OK;
}

void iddcxHealthCheck() {
	HANDLE hAdapter;

	bool manuallyCreated = false;
	int retries = 0;

	while (!manuallyCreated && retries < MAX_RETRIES) {
		HANDLE hAdapter = CreateFileW(MONIDROID_USER_DEVICE_PATH,
			GENERIC_READ | GENERIC_WRITE, 0,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr
		);

		if (!manuallyCreated) {
            HRESULT hr = CreateVirtualAdapter();
            if (FAILED(hr)) {
                // Monidroid::TaggedLog(TAG, )
            }
		}
	}
}
