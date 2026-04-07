#pragma once

#include <devioctl.h>
#include <initguid.h>
#include <d3dcommon.h>

#include "monidroid/edid.h"

typedef UINT_PTR SOCKET;

#define MONIDROID_DEVICE_PATH					L"\\Device\\MonidroidAdapter"
#define MONIDROID_USER_DEVICE_PATH				L"\\\\.\\GLOBALROOT" MONIDROID_DEVICE_PATH

#define MD_IOCTL_FUNC_BASE                      0xA10

#define MD_DEFINE_IOCTL(name, devtype, code, method, access, structIn, structOut) \
inline constexpr auto MD_IOCTL_ ## name    = code; \
inline constexpr auto IOCTL_IDDCX_ ## name = CTL_CODE(devtype, MD_IOCTL_FUNC_BASE + code, method, access); \
struct name ## _INFO_IN structIn; \
struct name ## _INFO_OUT structOut;

MD_DEFINE_IOCTL(MONITOR_CONNECT, FILE_DEVICE_VIDEO, 0, METHOD_BUFFERED, FILE_ANY_ACCESS, {
    unsigned int width;
    unsigned int height;
    unsigned int refreshRate;
}, {
    UINT connectorIndex;
    LUID adapterLuid;
    DWORD driverProcessId;
})

MD_DEFINE_IOCTL(MONITOR_DISCONNECT, FILE_DEVICE_VIDEO, 1, METHOD_BUFFERED, FILE_ANY_ACCESS, {
    UINT connectorIndex;
}, { })

MD_DEFINE_IOCTL(REQUEST_FRAME, FILE_DEVICE_VIDEO, 2, METHOD_BUFFERED, FILE_ANY_ACCESS, {
    UINT connectorIndex;
}, {
    LUID adapterLuid;
    HANDLE frameHandle;
    bool enabled;
    struct METADATA {
        UINT64 timeStamp;
        UINT frameNumber;
        Monidroid::MonitorMode mode;
    } metadata;
})

MD_DEFINE_IOCTL(FAST_REQUEST_FRAME, FILE_DEVICE_VIDEO, 3, METHOD_OUT_DIRECT, FILE_ANY_ACCESS, {
    UINT connectorIndex;
}, { })

// {ADF1470D-14D2-46AD-9499-896FDD87E215}
DEFINE_GUID(MonidroidGroupGuid, 0xadf1470d, 0x14d2, 0x46ad, 0x94, 0x99, 0x89, 0x6f, 0xdd, 0x87, 0xe2, 0x15);

struct ADAPTER_MONITOR_INFO_IN {
    unsigned int width;
    unsigned int height;
    unsigned int hertz;
};

struct ADAPTER_MONITOR_INFO_OUT {
    UINT connectorIndex;
    LUID adapterLuid;
    DWORD driverProcessId;
};

struct ADAPTER_MONITOR_INFO {
    _In_ SOCKET monitorNumberBySocket; // IN
    _In_ unsigned int width;           // IN
    _In_ unsigned int height;          // IN
    _In_ unsigned int hertz;           // IN

    _Out_ UINT connectorIndex;         // OUT
    _Out_ LUID adapterLuid;            // OUT
    _Out_ DWORD driverProcessId;       // OUT
};

struct FRAME_MONITOR_INFO {
    _In_ UINT connectorIndex;  // IN

    _Out_ LUID adapterLuid;    // OUT
    _Out_ HANDLE frameHandle;  // OUT
    _Out_ bool enabled;        // OUT 
    _Out_ struct METADATA {
        UINT64 timeStamp;
        UINT frameNumber;
        Monidroid::MonitorMode mode;
    } metadata;                // OUT
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

inline bool operator==(const LUID& lhs, const LUID& rhs) {
    return lhs.LowPart  == rhs.LowPart
        && lhs.HighPart == rhs.HighPart;
}
