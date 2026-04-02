#pragma once

#include <memory>
#include <system_error>

#include <Windows.h>
#include <wdf.h>
#include <IddCx.h>
#include <wrl.h>

#include "monidroid.h"
#include "monidroid/edid.h"
#include "iddcx.h"

using namespace Microsoft::WRL;
using namespace Monidroid;

class MonitorProcessor {
    using BufferArgs = IDARG_OUT_RELEASEANDACQUIREBUFFER;
    using Bool = unsigned int;
public:
    static constexpr auto MAX_STOP_WAIT = 10'000U;
    static constexpr auto MAX_FRAME_WAIT = 5'000U;

    MonitorProcessor(IDDCX_SWAPCHAIN swapChain, HANDLE hNextSurfaceAvailable);
    ~MonitorProcessor();

    MD_CLASS_PTR_ONLY(MonitorProcessor);

    HRESULT Init(LUID adapterLuid);
    HRESULT Start();
    void Stop() noexcept(false);
    HRESULT RequestFrame(FRAME_MONITOR_INFO& info);
    HRESULT CopyFrame(const BufferArgs& args);

private:
    static DWORD CALLBACK ThreadProc(void* arg);
    HRESULT StartPrivate();

private:
    IDDCX_SWAPCHAIN m_swapChain;
    HANDLE m_newFrameEvent;
    HANDLE m_thread;
    HANDLE m_stopEvent;

    Bool m_frameRequested;
    HANDLE m_frameReadyEvent;
    FRAME_MONITOR_INFO m_currentFrameInfo;

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
