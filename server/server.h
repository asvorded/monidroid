#pragma once

#include <memory>
#include <thread>
#include <set>
#include <mutex>
#include <chrono>
#include <functional>

#include "client.h"
#include "native.h"

#include <boost/asio.hpp>

using namespace boost::asio;

struct ClientContext {
    int id;
    ip::tcp::endpoint address;
    std::chrono::system_clock::time_point connectedAt;
    std::shared_ptr<Client> client;
    std::thread thread;
};

struct Notifier {
    std::function<void(std::shared_ptr<ClientContext>)> onClientConnected;
    std::function<void(std::shared_ptr<ClientContext>)> onClientDisconnected;
};

class Server {
    using ClientsSet = std::set<std::shared_ptr<ClientContext>>;

private:
    static constexpr auto TAG = "Server";
    
    ip::tcp::resolver m_resolver;
    ip::tcp::acceptor m_acceptor;
    bool m_running = false;

    ClientsSet m_clients;
    static int g_currentId;
    std::mutex lock;

    Notifier m_notifier;

    Adapter m_adapter;

    void serverMainAsync();

    void communicationMain(std::shared_ptr<ClientContext> ctx);

public:
    Server(io_context &context, Notifier notifier);
    ~Server();

    bool running() const;
    ClientsSet clients();
};
