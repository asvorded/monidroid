#pragma once

#include <memory>
#include <thread>
#include <set>
#include <mutex>

#include "client.h"
#include "native.h"

#include <boost/asio.hpp>

using namespace boost::asio;

class Server {
private:
    static constexpr auto TAG = "Server";
    
    ip::tcp::acceptor m_acceptor;
    bool m_running = false;

    std::set<std::shared_ptr<Client>> clients;
    std::mutex lock;

    Adapter m_adapter;

    void serverMainAsync();

    void communicationMain(std::shared_ptr<Client> client);

public:
    Server(boost::asio::io_context &context);
    ~Server();

    bool running() const;
};
