#pragma once

#include <devioctl.h>
#include <initguid.h>
#include <d3dcommon.h>

typedef UINT_PTR SOCKET;

#define MONIDROID_DEVICE_PATH					L"\\Device\\MonidroidAdapter"
#define MONIDROID_USER_DEVICE_PATH				L"\\\\.\\GLOBALROOT\\Device\\MonidroidAdapter"

#define IOCTL_IDDCX_MONITOR_CONNECT				CTL_CODE(FILE_DEVICE_VIDEO, 0xA10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_MONITOR_DISCONNECT			CTL_CODE(FILE_DEVICE_VIDEO, 0xA11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDDCX_REQUEST_FRAME				CTL_CODE(FILE_DEVICE_VIDEO, 0xA12, METHOD_BUFFERED, FILE_ANY_ACCESS)

// define for new requesting frames approach
#define MD_SYNC_REQUEST 1

// {ADF1470D-14D2-46AD-9499-896FDD87E215}
DEFINE_GUID(MonidroidGroupGuid, 0xadf1470d, 0x14d2, 0x46ad, 0x94, 0x99, 0x89, 0x6f, 0xdd, 0x87, 0xe2, 0x15);

struct ADAPTER_MONITOR_INFO {
    _In_ SOCKET monitorNumberBySocket; // IN
    _In_ int width;  // IN
    _In_ int height; // IN
    _In_ int hertz;  // IN

    _Out_ UINT connectorIndex; // OUT
    _Out_ LUID adapterLuid;    // OUT
    _Out_ DWORD driverProcessId; // OUT
};

struct FRAME_MONITOR_INFO {
    _In_ UINT connectorIndex;   // IN

    _Out_ HANDLE frameHandle;  // OUT
    _Out_ UINT64 timeStamp;
};

const int FEATURE_LEVELS_COUNT = 7;
const D3D_FEATURE_LEVEL FEATURE_LEVELS[FEATURE_LEVELS_COUNT] = {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,
    D3D_FEATURE_LEVEL_9_2,
    D3D_FEATURE_LEVEL_9_1,
};
