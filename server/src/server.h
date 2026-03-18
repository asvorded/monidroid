#pragma once

#include <memory>
#include <thread>

#include "client.h"

#include <boost/asio.hpp>

using namespace boost::asio;

class Server {
private:
    ip::tcp::acceptor m_acceptor;
    bool m_started = false;
    std::thread m_thread;

    void serverMain();
    void serverMainAsync();

    static void communicationMain(std::shared_ptr<Client> client);

public:
    Server(boost::asio::io_context &context);
    ~Server();

    void start();
    void stop();
};
