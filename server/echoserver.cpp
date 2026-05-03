#include "echoserver.h"

#include <array>
#include <iostream>
#include <format>
#include <sstream>
#include <fstream>

#include "monidroid.h"
#include "monidroid/protocol.h"
#include "monidroid/logger.h"

EchoServer::EchoServer(asio::io_context &context) 
  : m_socket(context),
    m_echoMessage(makeEchoMessage()),
    m_recvbuf(Monidroid::CL_ECHO_WORD),
    m_started(false)
{
    auto executor = m_socket.get_executor();
    auto endpoint = ip::udp::endpoint(ip::udp::v4(), Monidroid::PROTOCOL_PORT);
    m_socket = ip::udp::socket(executor, endpoint);
    
    m_started = true;
    Monidroid::TaggedLog(TAG, "Echo server started");
    
    echoMainAsync();
}

EchoServer::~EchoServer() {
    m_socket.close();
    
    if (m_echoThread.joinable()) {
        m_echoThread.join();
    }
    
    m_started = false;
    Monidroid::TaggedLog(TAG, "Echo server stopped");
}

bool EchoServer::running() const {
    return m_started;
}

void EchoServer::echoMainAsync() {
    m_socket.async_receive_from(boost::asio::buffer(m_recvbuf), m_from,
        [this](const boost::system::error_code& ec, std::size_t bytesReceived) {
            if (!ec) {
                if (bytesReceived == m_recvbuf.size() && m_recvbuf == Monidroid::CL_ECHO_WORD) {
                    boost::system::error_code ec;
                    size_t bytesSent = m_socket.send_to(boost::asio::buffer(m_echoMessage), m_from, 0, ec);
                    if (bytesSent == 0 || ec) {
                        Monidroid::TaggedLog(TAG, "sendto() failed, error: {}", ec.message());
                        return;
                    }
                }
                
                echoMainAsync();
            } else {
                if (ec != asio::error::operation_aborted) {
                    Monidroid::TaggedLog(TAG, "recvfrom() failed, error: {}", ec.message());
                }
                m_started = false;
            }
        }
    );
}

std::vector<char> EchoServer::makeEchoMessage() {
    std::string_view svWord(Monidroid::SV_ECHO_WORD);
    std::string svHostname(asio::ip::host_name());
    int length = svHostname.size();

    std::vector<char> out;
    out.reserve(svWord.size() + 3 + sizeof(length) + svHostname.size());
    
    // "SECHO"
    out.insert(out.end(), svWord.cbegin(), svWord.cend());

    // OS ID
    constexpr std::string_view osId(MD_OS_ID);
    static_assert(osId.size() == 3);
    out.insert(out.end(), osId.begin(), osId.end());
    
    // Hostname length
    const char *pLength = reinterpret_cast<const char*>(&length);
    out.insert(out.end(), pLength, pLength + sizeof(length));
    
    // Hostname
    out.insert(out.end(), svHostname.cbegin(), svHostname.cend());
    
    return out;
}
