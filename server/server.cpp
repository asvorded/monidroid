#include "server.h"

#include "monidroid.h"
#include "monidroid/logger.h"

constexpr auto TAG = "Server";

Server::Server(boost::asio::io_context &context)
    : m_acceptor(context, ip::tcp::endpoint(ip::tcp::v4(), Monidroid::MONIDROID_PORT))
{
    m_adapter = openAdapter();
    if (!m_adapter) {
        throw std::runtime_error("Unexpected inaccessibility of Monidroid Graphics Adapter");
    }

    serverMainAsync();

    m_running = true;
    Monidroid::TaggedLog(TAG, "Server started");
}

Server::~Server() {
    m_acceptor.close();
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
        
    Monidroid::TaggedLog("Server", "Server stopped");
}

bool Server::running() const {
    return m_running;
}

void Server::serverMain() {
    while (m_running) {
        boost::system::error_code ec;

        auto clientSocket = m_acceptor.accept(ec);
        if (!ec) {
            auto client = std::make_shared<Client>(std::move(clientSocket));
            
            // TODO: make list of clients
            client->m_communicationThread = std::thread([this, client]() {
                communicationMain(client);
            });
            client->m_communicationThread.detach();
        } else {
            Monidroid::TaggedLog("Server", "Ended accepting connections (message: {})", ec.message());
            break;
        }
    }
}

void Server::serverMainAsync() {
    // TODO: ensure proper `this` lifetime
    m_acceptor.async_accept([this](const boost::system::error_code &ec, ip::tcp::socket clientSocket) {
        if (!ec) {
            auto client = std::make_shared<Client>(std::move(clientSocket));
            
            // TODO: make list of clients
            client->m_communicationThread = std::thread([this, client]() {
                communicationMain(client);
            });
            client->m_communicationThread.detach();

            serverMainAsync();
        } else {
            Monidroid::TaggedLog("Server", "Ended accepting connections (message: {})", ec.message());
            m_running = false;
        }
    });
}

void Server::communicationMain(std::shared_ptr<Client> client) {
    Monidroid::DefaultLog("New client connected");

    // 1. Identify device
    client->identifyClient();

    // 2. Connect monitor
    client->connectMonitor(m_adapter);

    // 3. Send frames
    client->sendFrames();
    
    // 4. Disconnect monitor
    client->disconnectMonitor();

    // 5. Finalize
    client->finalize();

    Monidroid::DefaultLog("Client {} disconnected", client->m_modelName);
}
