#pragma once

#pragma warning(disable: 4471, justification: "C4471 appears when driver is compiled with C++20 standard")

#include "../MonidroidInfo/Monidroid.h"

#include <array>
//#include <memory>

#include <wdf.h>
#include <bugcodes.h>
#include <IddCx.h>
#include <Windows.h>
#include <wrl.h>
#include <WinSock2.h>
#include <wincodec.h>
#include <avrt.h>

/// <summary>
/// Context for adapter object
/// </summary>
struct AdapterContext {
    static const int MAX_MONITOR_COUNT = 5;

    AdapterContext(WDFDEVICE Device);
    ~AdapterContext();

    NTSTATUS Init();

    NTSTATUS DisconnectMonitor(ADAPTER_MONITOR_INFO* monitorInfo);
    NTSTATUS ConnectMonitor(ADAPTER_MONITOR_INFO* pMonitorInfo, bool edidProvided = false);
    NTSTATUS FrameRequest(FRAME_MONITOR_INFO* pFrameInfo);
    NTSTATUS FinishFrameRequest();

    //DWORD InitFrameSending(INIT_SEND_INFO* pSendInfo);
    //HRESULT SendNextFrame(UINT connectorIndex);
    //void FinalizeFrameSending(UINT connectorIndex);
private:
    struct MonitorLocalInfo {
        IDDCX_MONITOR monitorObject;
        SOCKET monitorNumberBySocket;
    } connectedMonitors[MAX_MONITOR_COUNT];

    WDFDEVICE device;
    IDDCX_ADAPTER adapter;
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

    // Sending frames by the driver
    //DWORD InitFrameSending(INIT_SEND_INFO* pSendInfo);
    //HRESULT SendNextFrame();
    //void FinalizeFrameSending();

    //NTSTATUS OnMonitorDisconnected();
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

    // sending by the driver
    SOCKET clientSocket = INVALID_SOCKET;

    // general info
    IDDCX_SWAPCHAIN swapChain { };
    LUID adapterLuid { };

    ID3D11Device3* pDevice = nullptr;
    ID3D11DeviceContext3* pDeviceContext = nullptr;

    HANDLE hProcessingThread { };
    HANDLE hNextFrameAvailable { };
    HANDLE hStopEvent { };
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
