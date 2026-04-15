#include "server.h"

#include "monidroid.h"
#include "monidroid/logger.h"

Server::Server(boost::asio::io_context &context)
    : m_acceptor(context, ip::tcp::endpoint(ip::tcp::v4(), Monidroid::MONIDROID_PORT))
{
    m_adapter = openAdapter();
    if (!m_adapter) {
        throw std::runtime_error("Unexpected graphics adapter inaccessibility detected during server initialization");
    }

    m_running = true;
    Monidroid::TaggedLog(TAG, "Server started");
    
    serverMainAsync();
}

Server::~Server() {
    boost::system::error_code ec;
    m_acceptor.cancel(ec);
    m_acceptor.close(ec);

    std::set<std::shared_ptr<Client>> copy;
    {
        std::lock_guard g(lock);
        copy = clients;
        clients.clear();
    }
    if (!copy.empty()) {
        Monidroid::TaggedLog(TAG, "Some clients are still connected, forcing disconnect...");
        for (auto& c : copy) {
            c->forceDisconnect();
            std::thread t = c->detachThread();
            if (t.joinable()) {
                t.join();
            }
        }
    }
        
    Monidroid::TaggedLog(TAG, "Server stopped");
}

bool Server::running() const {
    return m_running;
}

void Server::serverMainAsync() {
    m_acceptor.async_accept([this](const boost::system::error_code &ec, ip::tcp::socket clientSocket) {
        if (!ec) {
            auto client = std::make_shared<Client>(std::move(clientSocket));
            {
                std::lock_guard g(lock);
                clients.insert(client);
            }
            
            client->attachThread(std::thread([this, client]() {
                communicationMain(client);
            }));

            serverMainAsync();
        } else {
            Monidroid::TaggedLog(TAG, "Ended accepting connections (message: {})", ec.message());
            m_running = false;
        }
    });
}

void Server::communicationMain(std::shared_ptr<Client> client) {
    Monidroid::TaggedLog(TAG, "New client connected");

    bool result = false;

    // 1. Identify device
    result = client->identifyClient();
    if (!result) {
        Monidroid::TaggedLog(TAG, "Disconnected from client due to identification error");
        return;
    }

    // 2. Connect monitor
    result = client->connectMonitor(m_adapter);
    if (!result) {
        Monidroid::TaggedLog(TAG, "Failed to connect monitor, send error and disconnect");
        client->sendError(Monidroid::ErrorCode::MonitorConnectFail);
        return;
    }

    // 3. Send frames
    client->sendFrames();
    
    // 4. Disconnect monitor
    client->disconnectMonitor();

    {
        std::lock_guard g(lock);
        if (clients.erase(client)) {
            client->detachThread().detach();
        }
    }
    
    Monidroid::TaggedLog(TAG, "Client {} disconnected", client->modelName());
}
