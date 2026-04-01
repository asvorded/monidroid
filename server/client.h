#pragma once

#include <thread>
#include <vector>
#include <string>
#include <string_view>

#include <boost/asio.hpp>

#include "monidroid.h"

#include "native.h"

using namespace boost::asio;
using namespace Monidroid;

enum class ClientState {
    New, Identified, Connected, Streaming, Disconnected, Error
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

    std::thread m_communicationThread; // ???

    void allocFrameBuffer(int width, int height);
    void initPipeline();

    void grabAndSend(const std::unique_ptr<ColorType[]>& frameData, const int dataPixSize);

public:
    Client(ip::tcp::socket socket);
    ~Client();

    MD_CLASS_PTR_ONLY(Client);

    ClientState state() const;
    bool identifyClient();
    bool connectMonitor(const Adapter& adapter);
    void sendFrames();
    void disconnectMonitor();

    void sendError(ErrorCode code);
    void sendError(const std::string_view msg);
};