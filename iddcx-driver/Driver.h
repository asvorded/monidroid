#pragma once

#pragma warning(disable: 4471, justification: "C4471 appears when driver is compiled with C++20 standard")

#include <monidroid.h>
#include "iddcx.h"

#include <array>
#include <memory>

#include <wdf.h>
#include <bugcodes.h>
#include <IddCx.h>
#include <Windows.h>
#include <wrl.h>
#include <WinSock2.h>
#include <wincodec.h>
#include <avrt.h>

using Microsoft::WRL::ComPtr;

/// <summary>
/// Context for adapter object
/// </summary>
struct AdapterContext {
    static const int MAX_MONITOR_COUNT = Monidroid::MAX_MONITORS_SUPPORTED;

    AdapterContext(WDFDEVICE Device);
    ~AdapterContext();

    NTSTATUS Init();

    NTSTATUS ConnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo, bool edidProvided = false);
    NTSTATUS DisconnectMonitor(ADAPTER_MONITOR_INFO* monitorInfo);

    NTSTATUS FrameRequest(FRAME_MONITOR_INFO* pFrameInfo);
    NTSTATUS FinishFrameRequest();
private:
    struct MonitorLocalInfo {
        IDDCX_MONITOR monitorObject;
        SOCKET monitorNumberBySocket;
    } connectedMonitors[MAX_MONITOR_COUNT];

    WDFDEVICE device;
    IDDCX_ADAPTER adapter;
};

/// <summary>
/// Frames processor of a monitor
/// </summary>
class MonitorProcessor {
public:
    static constexpr auto REENCODES_COUNT = 1;

    MonitorProcessor(IDDCX_SWAPCHAIN swapChain, LUID adapterLuid, HANDLE hNextSurfaceAvailable);
    ~MonitorProcessor();

    HRESULT Start();
    HRESULT RequestFrame(FRAME_MONITOR_INFO& info);
    void Stop();

private:
    static constexpr int FEATURE_LEVELS_COUNT = 7;
    D3D_FEATURE_LEVEL FeatureLevels[FEATURE_LEVELS_COUNT] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    LUID m_adapterLuid;
    IDDCX_SWAPCHAIN m_swapChain;
    HANDLE m_hNewFrameEvent;
    HANDLE m_hStopEvent;

    ComPtr<ID3D11Device3> m_pDevice;
    ComPtr<IDXGIDevice3> m_pSwapChainDevice;
    ComPtr<ID3D11DeviceContext3> m_pDeviceContext;
};

/// <summary>
/// Context for monitor object
/// </summary>
struct MonitorContext {
    MonitorContext(IDDCX_MONITOR Monitor);
    ~MonitorContext();

    void SetupMonitor(ADAPTER_MONITOR_INFO* pMonitirInfo);

    HRESULT AssignSwapChain(IDDCX_SWAPCHAIN swapchain, LUID adapterLuid, HANDLE hNextSurfaceAvailable);

    // Frames processor control
    HRESULT InitProcessor();
    DWORD StartProcessor();
    DWORD StopProcessor();

    NTSTATUS UnassignSwapChain();
    void FinalizeProcessor();

    // Frames chain
    void PutFrameToChain(IDXGIResource1* pFrame);
    HRESULT GetFrameFromChain(HANDLE& phFrame);
    HRESULT FinishFrameRequest();

#ifdef MD_SYNC_REQUEST
    HRESULT RequestFrame(FRAME_MONITOR_INFO *pInfo);
#endif

private:
    static DWORD WINAPI MyThreadProc(LPVOID pContext);
    HRESULT ProcessorFunc();
    HRESULT ProcessorMain();

    static constexpr int FEATURE_LEVELS_COUNT = 7;
    D3D_FEATURE_LEVEL FeatureLevels[FEATURE_LEVELS_COUNT] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

    struct __info {
        IDDCX_MONITOR monitorObject;

        int width;
        int height;
        int hertz;
    } info;

    static constexpr int MAX_FRAMES_COUNT = 16;

    // frames chain
    CRITICAL_SECTION syncRoot;
    int currentFrameIndex { -1 };
    IDXGIResource1* framesChain[MAX_FRAMES_COUNT] { };
    IDXGIResource1* pRequestedFrame { };

    IDDCX_SWAPCHAIN swapChain { };
    LUID adapterLuid { };

    ID3D11Device3* pDevice = nullptr;
    ID3D11DeviceContext3* pDeviceContext = nullptr;

    HANDLE hProcessingThread { };
    HANDLE hNextFrameAvailable { };
    HANDLE hStopEvent { };

#ifdef MD_SYNC_REQUEST
    std::unique_ptr<MonitorProcessor> m_pProcessor;
#endif
};

struct IndirectMonitorInfo {
    static constexpr size_t szEdidBlock = 128;
    static constexpr size_t szModeList = 3;

    const BYTE pEdidBlock[szEdidBlock];
    const struct IndirectMonitorMode {
        DWORD Width;
        DWORD Height;
        DWORD VSync;
    } pModeList[szModeList];
    const DWORD ulPreferredModeIdx;
};
