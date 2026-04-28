#pragma once

#include <thread>
#include <vector>
#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "monidroid.h"
#include "monidroid/protocol.h"

#include "native.h"

using namespace boost;
using namespace boost::asio;
using namespace boost::system;
using namespace Monidroid;

enum class ClientState {
    New, Identified, Connected, Streaming, ConnectionClosed, Disconnected, Error
};

struct Client {
    static constexpr auto CLIENT_TAG = "Client";

private:
    ClientState m_state = ClientState::New;

    ip::tcp::socket m_socket;
    std::vector<char> m_netBuffer;
    std::jthread m_inputThread;

    MonitorMode m_preffered;
    std::string m_modelName;

    Monitor m_monitor;

    void sendFullFrame(const FrameMapInfo& info);
    void sendMonitorOff();

    void receiveMain();
    void handleInput();
public:
    Client(ip::tcp::socket socket);
    ~Client();

    MD_CLASS_PTR_ONLY(Client)

    const std::string& modelName() const;
    ClientState state() const;

    bool identifyClient();
    bool connectMonitor(const Adapter& adapter);
    void sendFrames();
    void disconnectMonitor();

    void forceDisconnect(bool withErrorCode = false);

    void sendError(ErrorCode code);
    void sendError(const std::string_view msg);
};