#pragma once

#include <thread>
#include <vector>
#include <string>

#include <boost/asio.hpp>

using namespace boost::asio;

class EchoServer {
private:
    ip::udp::socket m_socket;
    bool m_started;
    std::thread m_echoThread;

    std::vector<char> m_echoMessage;
    std::string m_recvbuf;
    ip::udp::endpoint m_from;

    void echoMainAsync();

    std::vector<char> makeEchoMessage();
public:
    EchoServer(io_context &context);
    ~EchoServer();

    void start();
    void stop();
};
