#include "server.h"

#include "monidroid.h"
#include "monidroid/logger.h"

int Server::g_currentId = 0;

Server::Server(io_context &context, Notifier notifier)
  : m_acceptor(context, ip::tcp::endpoint(ip::tcp::v4(), Monidroid::PROTOCOL_PORT)),
    m_resolver(context),
    m_adapter(openAdapter()),
    m_notifier(notifier)
{
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

    //                                ( this thread |     client's thread     )
    // Make copy to prevent deadlocks (   t.join()  | std::guard_lock g(lock) );
    std::set<std::shared_ptr<ClientContext>> copy;
    {
        std::lock_guard g(lock);
        copy = m_clients;
        m_clients.clear();
    }
    if (!copy.empty()) {
        Monidroid::TaggedLog(TAG, "Some clients are still connected, forcing disconnect...");
        for (auto& c : copy) {
            c->client->forceDisconnect();
            std::thread t = std::move(c->thread);
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

auto Server::clients() -> ClientsSet {
    std::lock_guard g(lock);
    return m_clients;
}

ServerInfo Server::serverInfo() {
    std::string hostname = asio::ip::host_name();

    std::vector<std::string> addrs;
    for (const auto &it : m_resolver.resolve(hostname, "http")) {
        auto addr = it.endpoint().address();
        if (addr.is_v4()) {
            addrs.push_back(addr.to_string());
        }
    }

    return { .hostname = hostname, .addrs = addrs };
}

void Server::serverMainAsync() {
    m_acceptor.async_accept([this](const boost::system::error_code &ec, ip::tcp::socket clientSocket) {
        if (!ec) {
            auto ctx = std::shared_ptr<ClientContext>(new ClientContext {
                .id = g_currentId++,
                .address = clientSocket.remote_endpoint(),
                .connectedAt = std::chrono::system_clock::now(),
                .client = std::make_shared<Client>(std::move(clientSocket)),
            });
            ctx->thread = std::thread([this, ctx]() {
                communicationMain(ctx);
            });

            serverMainAsync();
        } else {
            if (ec != asio::error::operation_aborted) {
                Monidroid::TaggedLog(TAG, "Ended accepting connections (message: {})", ec.message());
            }
            m_running = false;
        }
    });
}

void Server::communicationMain(std::shared_ptr<ClientContext> ctx) {
    Monidroid::TaggedLog(TAG, "New client connected");

    {
        std::lock_guard g(lock);
        m_clients.insert(ctx);
    }
    if (m_notifier.onClientConnected) {
        m_notifier.onClientConnected(ctx);
    }

    auto client = ctx->client;
    try {
        bool result = false;

        // 1. Identify device
        result = client->identifyClient();
        if (!result) {
            Monidroid::TaggedLog(TAG, "Disconnected from client due to identification error");
            return;
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
        
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

        Monidroid::TaggedLog(TAG, "Client {} disconnected", client->modelName());
    } catch (const std::runtime_error& e) {
        Monidroid::TaggedLog(client->modelName(), "{}", e.what());
        Monidroid::TaggedLog(client->modelName(), "Disconnecting due to critical error");
        client->sendError(e.what());
    }

    {
        std::lock_guard g(lock);
        if (m_clients.erase(ctx)) {
            // No lock(), because we are in ctx->thread
            ctx->thread.detach();
        }
    }
    if (m_notifier.onClientDisconnected) {
        m_notifier.onClientDisconnected(ctx);
    }
}
