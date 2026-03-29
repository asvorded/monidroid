#pragma once

#include <memory>
#include <thread>

#include "client.h"
#include "native.h"

#include <boost/asio.hpp>

using namespace boost::asio;

class Server {
private:
    ip::tcp::acceptor m_acceptor;
    bool m_running = false;
    std::thread m_thread;

    Adapter m_adapter;

    void serverMain();
    void serverMainAsync();

    void communicationMain(std::shared_ptr<Client> client);

public:
    Server(boost::asio::io_context &context);
    ~Server();

    bool running() const;
};
