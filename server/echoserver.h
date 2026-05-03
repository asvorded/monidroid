#pragma once

#include <thread>
#include <vector>
#include <string>

#include <boost/asio.hpp>

namespace asio = boost::asio;
namespace ip = boost::asio::ip;

class EchoServer {
    static constexpr auto TAG = "ECHO";

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
    EchoServer(asio::io_context &context);
    ~EchoServer();

    bool running() const;
};
