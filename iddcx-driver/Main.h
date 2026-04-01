#pragma once

#include <Windows.h>
#include <wdf.h>
#include <IddCx.h>

EVT_WDF_DRIVER_DEVICE_ADD       MdEvtDriverDeviceAdd;

EVT_WDF_DEVICE_D0_ENTRY         MdEvtDeviceD0Entry;
EVT_WDF_DEVICE_D0_EXIT          MdEvtDeviceD0Exit;

/* @brief Called when usermode calls to @c CreateFile() */
EVT_WDF_DEVICE_FILE_CREATE      MdEvtFileCreate;

/* @brief Called when usermode closes the file */
EVT_WDF_FILE_CLOSE              MdEvtFileClose;

/* @brief Called when usermode calls to @c DeviceIoControl() */
EVT_IDD_CX_DEVICE_IO_CONTROL    MdEvtDeviceIoControl;

/* @brief Called after adapter initialization */
EVT_IDD_CX_ADAPTER_INIT_FINISHED                    MdEvtAdapterInitFinished;

/* @brief Called to get base (target) monitor modes */
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES               MdEvtMonitorQueryTargetModes;

/* @brief Called to get monitor modes if no EDID is provided */
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES    MdEvtMonitorGetDefaultDescriptionModes;

/* @brief Called to get monitor modes if EDID is provided */
EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION                MdEvtParseMonitorDescription;

/* @brief Called when monitor modes are commited or changed */
EVT_IDD_CX_ADAPTER_COMMIT_MODES                     MdEvtAdapterCommitModes;

/* @brief Called when swap-chain is assigned or reassigned to monitor */
EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN                 MdEvtMonitorAssignSwapChain;

/* @brief Called when swap-chain is unassigned from monitor */
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN               MdEvtMonitorUnassignSwapChain;
