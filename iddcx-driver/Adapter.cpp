#include "Adapter.h"

#include "monidroid/edid.h"

#include "Monitor.h"

AdapterContext::AdapterContext(WDFDEVICE Device) : m_device(Device), m_adapter() {
    for (int i = 0; i < MAX_MONITOR_COUNT; i++) {
        connectedMonitors[i].monitorObject = nullptr;
    }
}

AdapterContext::~AdapterContext() { }

/// <summary>
/// Initializes display adapter
/// </summary>
NTSTATUS AdapterContext::Init() {
    IDDCX_ENDPOINT_VERSION version = { .Size = sizeof(version), .MajorVer = 0, .MinorVer = 1 };

    IDDCX_ADAPTER_CAPS adapterCaps = {
        .Size = sizeof(adapterCaps),

        .MaxMonitorsSupported = AdapterContext::MAX_MONITOR_COUNT,
        .EndPointDiagnostics {
            .Size = sizeof(adapterCaps.EndPointDiagnostics),
            .TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER,
            .pEndPointFriendlyName = L"<hidden>",
            .pEndPointModelName = L"<hidden>",
            .pEndPointManufacturerName = L"<hidden>",
            .pHardwareVersion = &version,
            .pFirmwareVersion = &version,
            .GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE, // #1
        },
        .StaticDesktopReencodeFrameCount = REENCODES_COUNT,
    };

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, AdapterContextWrapper);

    IDARG_IN_ADAPTER_INIT adapterInit {
        .WdfDevice = m_device,
        .pCaps = &adapterCaps,
        .ObjectAttributes = &attributes
    };
    IDARG_OUT_ADAPTER_INIT adapterInitOut = {};

    NTSTATUS status = IddCxAdapterInitAsync(&adapterInit, &adapterInitOut);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    this->m_adapter = adapterInitOut.AdapterObject;

    auto* pContext = WdfObjectGet_AdapterContextWrapper(adapterInitOut.AdapterObject);
    pContext->self = this;

    return STATUS_SUCCESS;
}

/// <summary>
/// Called when new monitor was connected
/// </summary>
NTSTATUS AdapterContext::ConnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo) {
    // Find the closest slot
    int idx = 0;
    while (connectedMonitors[idx].monitorObject != nullptr && idx < MAX_MONITOR_COUNT) idx++;
    if (idx == MAX_MONITOR_COUNT) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Fill monitor info
    IDDCX_MONITOR_INFO monitorInfo = {};
    monitorInfo.Size = sizeof(IDDCX_MONITOR_INFO);
    monitorInfo.ConnectorIndex = idx;
    monitorInfo.MonitorContainerId = MonidroidGroupGuid;
    monitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;

    monitorInfo.MonitorDescription.Size = sizeof(monitorInfo.MonitorDescription);

    auto edid = Monidroid::CUSTOM_EDID;
    edid.setDefaultMode(pMonitorInfo->width, pMonitorInfo->height, pMonitorInfo->hertz);
    edid.commit();

    monitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    if (EDID_PROVIDED) {
        monitorInfo.MonitorDescription.pData = edid.raw();
        monitorInfo.MonitorDescription.DataSize = sizeof(edid);
    } else {
        monitorInfo.MonitorDescription.pData = NULL;
        monitorInfo.MonitorDescription.DataSize = 0;
    }

    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, MonitorContextWrapper);
    attr.EvtCleanupCallback = [](WDFOBJECT object) {
        if (auto *context = WdfObjectGet_MonitorContextWrapper(object)) {
            context->Cleanup();
        }
    };

    IDARG_IN_MONITORCREATE monitorCreate {
        .ObjectAttributes = &attr,
        .pMonitorInfo = &monitorInfo
    };
    IDARG_OUT_MONITORCREATE monitorCreateOut;

    NTSTATUS status = IddCxMonitorCreate(m_adapter, &monitorCreate, &monitorCreateOut);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Set up monitor context
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorCreateOut.MonitorObject);
    pContext->self = new MonitorContext(monitorCreateOut.MonitorObject);

    // Add monitor to monitor list
    connectedMonitors[idx].monitorObject = monitorCreateOut.MonitorObject;
    pContext->self->SetupMonitor(pMonitorInfo);

    // Tell the OS that the monitor has been plugged in
    IDARG_OUT_MONITORARRIVAL monitorArrivalOut;
    status = IddCxMonitorArrival(monitorCreateOut.MonitorObject, &monitorArrivalOut);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Pass data to enable frames processing
    pMonitorInfo->connectorIndex = idx;
    pMonitorInfo->adapterLuid = monitorArrivalOut.OsAdapterLuid;
    pMonitorInfo->driverProcessId = GetCurrentProcessId();

    return status;
}

NTSTATUS AdapterContext::DisconnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo) {
    // Find slot
    int idx = pMonitorInfo->connectorIndex;
    if (connectedMonitors[idx].monitorObject == nullptr) {
        return STATUS_NOT_FOUND;
    }

    IDDCX_MONITOR m = connectedMonitors[idx].monitorObject;
    connectedMonitors[idx].monitorObject = nullptr;

    return IddCxMonitorDeparture(m);
}

NTSTATUS AdapterContext::FrameRequest(FRAME_MONITOR_INFO* pFrameInfo) {
    // get handle to frame
    IDDCX_MONITOR monitorObject = connectedMonitors[pFrameInfo->connectorIndex].monitorObject;
    auto* pContext = WdfObjectGet_MonitorContextWrapper(monitorObject);

    HRESULT hr = pContext->self->RequestFrame(*pFrameInfo);
    if (FAILED(hr)) {
        return NTSTATUS_FROM_WIN32(HRESULT_CODE(hr));
    }

    return STATUS_SUCCESS;
}
