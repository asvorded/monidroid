#pragma once

#include <memory>

#include <Windows.h>
#include <wdf.h>
#include <IddCx.h>
#include <wrl.h>

#include "monidroid/edid.h"
#include "iddcx.h"

using namespace Microsoft::WRL;
using namespace Monidroid;

class MonitorProcessor {
public:
    MonitorProcessor(IDDCX_SWAPCHAIN swapChain, LUID adapterLuid, HANDLE hNextSurfaceAvailable);
    ~MonitorProcessor();

    HRESULT Start();
    HRESULT RequestFrame(FRAME_MONITOR_INFO& info);
    void Stop();

private:
    IDDCX_SWAPCHAIN m_swapChain;
    HANDLE m_hNewFrameEvent;
    HANDLE m_hStopEvent;

    ComPtr<ID3D11Device3> m_pDevice;
    ComPtr<ID3D11DeviceContext3> m_pDeviceContext;
};

struct MonitorContext {
    MonitorContext(IDDCX_MONITOR Monitor);
    ~MonitorContext();

    MonitorContext(const MonitorContext&) = delete;
    MonitorContext& operator=(const MonitorContext&) = delete;

    void SetupMonitor(const ADAPTER_MONITOR_INFO* pMonitirInfo);
    const MonitorMode& PreferredMode() const;

    HRESULT AssignSwapChain(IDDCX_SWAPCHAIN swapchain, LUID adapterLuid, HANDLE hNextSurfaceAvailable);
    void UnassignSwapChain();

    HRESULT RequestFrame(FRAME_MONITOR_INFO* pInfo);

private:
    IDDCX_MONITOR m_monitor;
    MonitorMode m_preffered;

    std::unique_ptr<MonitorProcessor> m_pProcessor;
};

struct MonitorContextWrapper {
    MonitorContext* self;

    void Cleanup() {
        delete self;
        self = nullptr;
    }
};

WDF_DECLARE_CONTEXT_TYPE(MonitorContextWrapper);
