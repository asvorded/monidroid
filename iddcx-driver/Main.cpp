#include "Main.h"

#include "iddcx.h"
#include "monidroid/edid.h"

#include "Adapter.h"
#include "Monitor.h"

using namespace Monidroid;

static inline DISPLAYCONFIG_VIDEO_SIGNAL_INFO MakeSignalInfo(const MonitorMode& mode, UINT32 vSyncFreqDivider = 0) {
    return {
        .pixelRate = mode.width * mode.height * mode.refreshRate,
        .hSyncFreq { .Numerator = mode.refreshRate * mode.height, .Denominator = 1 },
        .vSyncFreq { .Numerator = mode.refreshRate, .Denominator = 1 },
        .activeSize { .cx = mode.width, .cy = mode.height },
        .totalSize { .cx = mode.width, .cy = mode.height },
        .AdditionalSignalInfo {
            // <d3dkmdt.h> generally cannot be included in UMD, so enum value is replaced
            .videoStandard = 3, // D3DKMDT_VSS_VESA_CVT
            .vSyncFreqDivider = vSyncFreqDivider,
        },
        .scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE
    };
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, UINT dwReason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);
    UNREFERENCED_PARAMETER(dwReason);

    return TRUE;
}

extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObject, IN PUNICODE_STRING registryPath) {
    NTSTATUS status;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, MdEvtDriverDeviceAdd);

    WDFDRIVER driver;

    status = WdfDriverCreate(driverObject, registryPath, &attributes, &config, &driver);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS MdEvtDriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);

    /* Initialize PnP callbacks */ 
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);

    pnpCallbacks.EvtDeviceD0Entry = MdEvtDeviceD0Entry;
    pnpCallbacks.EvtDeviceD0Exit  = MdEvtDeviceD0Exit;

    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    /* Initialize IddCx */ 
    IDD_CX_CLIENT_CONFIG config;
    IDD_CX_CLIENT_CONFIG_INIT(&config);

    config.EvtIddCxDeviceIoControl                   = MdEvtDeviceIoControl;

    config.EvtIddCxAdapterInitFinished               = MdEvtAdapterInitFinished;

    config.EvtIddCxMonitorQueryTargetModes           = MdEvtMonitorQueryTargetModes;
    config.EvtIddCxMonitorGetDefaultDescriptionModes = MdEvtMonitorGetDefaultDescriptionModes;
    config.EvtIddCxParseMonitorDescription           = MdEvtParseMonitorDescription;
    config.EvtIddCxAdapterCommitModes                = MdEvtAdapterCommitModes;

    config.EvtIddCxMonitorAssignSwapChain            = MdEvtMonitorAssignSwapChain;
    config.EvtIddCxMonitorUnassignSwapChain          = MdEvtMonitorUnassignSwapChain;

    NTSTATUS status = IddCxDeviceInitConfig(DeviceInit, &config);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Register file handling */ 
    WDF_FILEOBJECT_CONFIG fileConfig;
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, MdEvtFileCreate, MdEvtFileClose, WDF_NO_EVENT_CALLBACK);

    WDF_OBJECT_ATTRIBUTES fileAttrs;
    WDF_OBJECT_ATTRIBUTES_INIT(&fileAttrs);
    fileAttrs.SynchronizationScope = WdfSynchronizationScopeNone;

    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &fileAttrs);

    /* Create device */
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, AdapterContextWrapper);
    attributes.EvtCleanupCallback = [](WDFOBJECT object) {
        if (AdapterContextWrapper* wrapper = WdfObjectGet_AdapterContextWrapper(object)) {
            wrapper->Cleanup();
        }
    };

    WDFDEVICE device;
    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Register device symbolic link */
    DECLARE_CONST_UNICODE_STRING(symbolicLink, MONIDROID_DEVICE_PATH);

    status = WdfDeviceCreateSymbolicLink(device, &symbolicLink);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    /* Finish initialization */
    AdapterContextWrapper* pContext = WdfObjectGet_AdapterContextWrapper(device);
    pContext->self = new AdapterContext(device);

    status = IddCxDeviceInitialize(device);

    return status;
}

NTSTATUS MdEvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState) {
    UNREFERENCED_PARAMETER(PreviousState);

    auto* context = WdfObjectGet_AdapterContextWrapper(Device);
    NTSTATUS status = context->self->Init();

    return status;
}

NTSTATUS MdEvtDeviceD0Exit(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(TargetState);

    return STATUS_SUCCESS;
}

void MdEvtFileCreate(WDFDEVICE Device, WDFREQUEST Request, WDFFILEOBJECT FileObject) {
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

void MdEvtFileClose(WDFFILEOBJECT FileObject) {
    UNREFERENCED_PARAMETER(FileObject);
}

void MdEvtDeviceIoControl(WDFDEVICE Device, WDFREQUEST Request,
    size_t OutputBufferLength, size_t InputBufferLength,
    ULONG IoControlCode
) {
    auto* pContext = WdfObjectGet_AdapterContextWrapper(Device);

    WDFMEMORY inMemory = nullptr;
    WDFMEMORY outMemory = nullptr;

    WdfRequestRetrieveInputMemory(Request, &inMemory);
    WdfRequestRetrieveOutputMemory(Request, &outMemory);

    PVOID inBuf = WdfMemoryGetBuffer(inMemory, NULL);

    NTSTATUS status = STATUS_INVALID_PARAMETER;
    ULONG_PTR information = 0;

#define MD_ADD_IOCTL_HANDLER(code, func, type) \
case code: \
{ \
    type* data = (type*)inBuf; \
    status = pContext->self->func(data); \
    WdfMemoryCopyFromBuffer(outMemory, 0, inBuf, sizeof(type)); \
    information = sizeof(type); \
    break; \
}

    switch (IoControlCode) {
    MD_ADD_IOCTL_HANDLER(IOCTL_IDDCX_MONITOR_CONNECT, AdapterContext::ConnectMonitor, ADAPTER_MONITOR_INFO)
    MD_ADD_IOCTL_HANDLER(IOCTL_IDDCX_MONITOR_DISCONNECT, AdapterContext::DisconnectMonitor, ADAPTER_MONITOR_INFO)
    MD_ADD_IOCTL_HANDLER(IOCTL_IDDCX_REQUEST_FRAME, AdapterContext::FrameRequest, FRAME_MONITOR_INFO)
    default:
        status = STATUS_INVALID_PARAMETER;
        break;
    }

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    WdfRequestCompleteWithInformation(Request, status, information);
}

NTSTATUS MdEvtAdapterInitFinished(
    IDDCX_ADAPTER AdapterObject,
    const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs
) {
    UNREFERENCED_PARAMETER(AdapterObject);

    return pInArgs->AdapterInitStatus;
}

NTSTATUS MdEvtParseMonitorDescription(
    const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs,
    IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs
) {
    const UINT modesCount = ARRAYSIZE(CUSTOM_EDID_MODES);

    if (pInArgs->MonitorModeBufferInputCount < modesCount) {
        pOutArgs->MonitorModeBufferOutputCount = modesCount;
        return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }

    auto& timing = ((const EDID*)(pInArgs->MonitorDescription.pData))->dataBlocks[0].timing;
    auto vBlank = timing.vblank_lo | ((timing.vactive_vblank_hi & 0x0Fu) << 8);

    MonitorMode preferred {
        .width = timing.hactive_lo | ((timing.hactive_hblank_hi & 0xF0u) << 4),
        .height = timing.vactive_lo | ((timing.vactive_vblank_hi & 0xF0u) << 4),
        .refreshRate = (timing.pixel_clock * 10'000u) / ((preferred.width + EDID_H_BLANK) * (preferred.height + vBlank)),
    };

    for (int i = 0; i < modesCount; ++i) {
        const MonitorMode& mode = i == 0 ? preferred : Monidroid::CUSTOM_EDID_MODES[i];

        pInArgs->pMonitorModes[i] = {
            .Size = sizeof(*pInArgs->pMonitorModes),
            .Origin = IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR,
            .MonitorVideoSignalInfo = MakeSignalInfo(mode),
        };
    }
    pOutArgs->PreferredMonitorModeIdx = 0;

    return STATUS_SUCCESS;
}

NTSTATUS MdEvtMonitorGetDefaultDescriptionModes(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs,
    IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs
) {
    auto* context = WdfObjectGet_MonitorContextWrapper(MonitorObject);
    const UINT modesCount = ARRAYSIZE(CUSTOM_EDID_MODES);

    pOutArgs->DefaultMonitorModeBufferOutputCount = modesCount;

    if (pInArgs->DefaultMonitorModeBufferInputCount < modesCount) {
        return (pInArgs->DefaultMonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }

    for (DWORD i = 0; i < ARRAYSIZE(CUSTOM_EDID_MODES); i++) {
        auto mode = i == 0 ? context->self->PreferredMode() : CUSTOM_EDID_MODES[i];
        pInArgs->pDefaultMonitorModes[i] = {
            .Size = sizeof(*pInArgs->pDefaultMonitorModes),
            .Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER,
            .MonitorVideoSignalInfo = MakeSignalInfo(mode),
        };
    }

    pOutArgs->PreferredMonitorModeIdx = 0;
    
    return STATUS_SUCCESS;
}

NTSTATUS MdEvtMonitorQueryTargetModes(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_QUERYTARGETMODES* pInArgs,
    IDARG_OUT_QUERYTARGETMODES* pOutArgs
) {
    auto* context = WdfObjectGet_MonitorContextWrapper(MonitorObject);
    const UINT modesCount = ARRAYSIZE(CUSTOM_EDID_MODES);

    pOutArgs->TargetModeBufferOutputCount = modesCount;

    if (pInArgs->TargetModeBufferInputCount < modesCount) {
        return (pInArgs->TargetModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }

    for (int i = 0; i < 3; i++) {
        auto mode = i == 0 ? context->self->PreferredMode() : CUSTOM_EDID_MODES[i];
        pInArgs->pTargetModes[i] = {
            .Size = sizeof(*pInArgs->pTargetModes),
            .TargetVideoSignalInfo {
                .targetVideoSignalInfo = MakeSignalInfo(mode, 1),
            },
        };
    }

    return STATUS_SUCCESS;
}

NTSTATUS MdEvtAdapterCommitModes(
    IDDCX_ADAPTER AdapterObject,
    const IDARG_IN_COMMITMODES* pInArgs
) {
    UNREFERENCED_PARAMETER(AdapterObject);
    UNREFERENCED_PARAMETER(pInArgs);

    return STATUS_SUCCESS;
}

NTSTATUS MdEvtMonitorAssignSwapChain(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_SETSWAPCHAIN* pInArgs
) {
    auto* context = WdfObjectGet_MonitorContextWrapper(MonitorObject);
    HRESULT hr = context->self->AssignSwapChain(
        pInArgs->hSwapChain, pInArgs->RenderAdapterLuid, pInArgs->hNextSurfaceAvailable
    );
    if (FAILED(hr)) {
        return STATUS_GRAPHICS_INDIRECT_DISPLAY_ABANDON_SWAPCHAIN;
    }

    return STATUS_SUCCESS;
}

NTSTATUS MdEvtMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject) {
    auto* context = WdfObjectGet_MonitorContextWrapper(MonitorObject);
    context->self->UnassignSwapChain();

    return STATUS_SUCCESS;
}