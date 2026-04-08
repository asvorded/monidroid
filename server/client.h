#pragma once

#include <thread>
#include <vector>
#include <string>
#include <string_view>

#include <evdi_lib.h>
#include <boost/asio.hpp>

#include "monidroid.h"

#include "native.h"

using namespace boost::asio;
using namespace Monidroid;

enum class ClientState {
    New, Identified, Connected, Streaming, ConnectionClosed, Disconnected, Error
};

struct Client {
    static constexpr auto CLIENT_TAG = "Client";

public:
    ip::tcp::socket m_socket;
    std::vector<char> m_netBuffer;
    bool m_sending = false;

    MonitorMode m_preffered;
    std::string m_modelName;
    ClientState m_state = ClientState::New;
    
    Monitor m_monitor;
    
    evdi_handle m_handle = EVDI_INVALID_HANDLE;
    int m_devNumber = 0;

    evdi_buffer m_frameBufferInfo;
    std::vector<ColorType> m_frameBuffer;

    std::thread m_communicationThread; // ???

    void allocFrameBuffer(int width, int height);
    void initPipeline();

    void sendFullFrame(const FrameMapInfo& info);
    int grabAndSend(int bufferId);

    static void dpmsHandler(int dpms_mode, void *user_data);
    static void modeHandler(struct evdi_mode mode, void *user_data);
    static void updateHandler(int buffer_to_be_updated, void *user_data);

public:
    Client(ip::tcp::socket socket);
    ~Client();

    MD_CLASS_PTR_ONLY(Client)

    ClientState state() const;
    bool identifyClient();
    bool connectMonitor(const Adapter& adapter);
    void sendFrames();
    void disconnectMonitor();

    void sendMonitorOff();
    void sendError(ErrorCode code);
    void sendError(const std::string_view msg);
};