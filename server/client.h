#pragma once

#include <thread>
#include <vector>
#include <string>
#include <string_view>

#include <boost/asio.hpp>
#include <x264.h>

#include "monidroid.h"
#include "monidroid/protocol.h"

#include "native.h"

using namespace std::chrono;
using namespace boost;
using namespace boost::asio;
using namespace boost::system;
using namespace Monidroid;

enum class ClientState {
    New, Identified, Connected, Streaming, ConnectionClosed, Disconnected, Error
};

struct Client {
    static constexpr auto CLIENT_TAG = "Client";

    struct TimeLogEntry {
        double time_point;
        double duration1;
        double duration2;
    };

private:
    ClientState m_state = ClientState::New;

    ip::tcp::socket m_socket;
    std::vector<char> m_netBuffer;
    std::jthread m_inputThread;
    steady_clock::time_point m_syncTime;
    x264_t *m_codec;
    x264_picture_t m_pic;

    MonitorMode m_preffered;
    std::string m_modelName;
    bool m_isUsb;

    Monitor m_monitor;

    bool sync();

    bool initEncoder(const MonitorMode &mode);

    bool reconfigureEncoder(const MonitorMode &mode);

    void sendFullFrame(const FrameMapInfo& info, const FrameMetadata& meta);
    void sendStreamFrame(const FrameMapInfo& info, const FrameMetadata& meta);
    void sendMonitorOff();

    void receiveMain();
    void handleInput();
public:
    explicit Client(ip::tcp::socket socket);
    ~Client();

    MD_CLASS_PTR_ONLY(Client)

    const std::string& modelName() const;
    ClientState state() const;
    bool isUsb() const;

    bool identifyClient();
    bool connectMonitor(const Adapter& adapter);
    void sendFrames();
    void disconnectMonitor();

    void forceDisconnect(bool withErrorCode = false);

    void sendError(ErrorCode code);
    void sendError(std::string_view msg);
};