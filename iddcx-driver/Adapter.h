#pragma once

#include <Windows.h>
#include <wdf.h>
#include <IddCx.h>

#include "monidroid.h"
#include "iddcx.h"

struct AdapterContext {
    static constexpr auto EDID_PROVIDED = true;
    static constexpr auto MAX_MONITOR_COUNT = Monidroid::MAX_MONITORS_SUPPORTED;
    static constexpr auto REENCODES_COUNT = 1;

    AdapterContext(WDFDEVICE Device);
    ~AdapterContext();

    AdapterContext(const AdapterContext&) = delete;
    AdapterContext& operator=(const AdapterContext&) = delete;

    NTSTATUS Init();

    NTSTATUS ConnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo);
    NTSTATUS DisconnectMonitor(ADAPTER_MONITOR_INFO* monitorInfo);

    NTSTATUS FrameRequest(FRAME_MONITOR_INFO* pFrameInfo);
private:
    struct MonitorLocalInfo {
        IDDCX_MONITOR monitorObject;
    } connectedMonitors[MAX_MONITOR_COUNT];

    WDFDEVICE m_device;
    IDDCX_ADAPTER m_adapter;
};

struct AdapterContextWrapper {
    AdapterContext* self;

    void Cleanup() {
        delete self;
        self = nullptr;
    }
};

WDF_DECLARE_CONTEXT_TYPE(AdapterContextWrapper);
