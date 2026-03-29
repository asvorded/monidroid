#pragma once

#include <thread>
#include <vector>
#include <string>

#include "native.h"

#include <boost/asio.hpp>

using namespace boost::asio;

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

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    ClientState state() const;
    bool identifyClient();
    bool connectMonitor(const Adapter& adapter);
    void sendFrames();
    void disconnectMonitor();
    void finalize();
};
