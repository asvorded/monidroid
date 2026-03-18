#include "server.h"

#include <monidroid.h>
#include "logger.h"

Server::Server(boost::asio::io_context &context) : m_acceptor(context) {}

Server::~Server() {
    stop();
}

void Server::start() {
    if (m_started) return;

    auto executor = m_acceptor.get_executor();
    auto endpoint = ip::tcp::endpoint(ip::tcp::v4(), Monidroid::MONIDROID_PORT);
    m_acceptor = ip::tcp::acceptor(executor, endpoint);

    // m_thread = std::thread([this]() { serverMain(); });
    serverMainAsync();

    m_started = true;
    Monidroid::TaggedLog("Server", "Server started");
}

void Server::stop() {
    if (!m_started) return;

    m_acceptor.close();
    
    if (m_thread.joinable()) {
        m_thread.join();
    }

    m_started = false;
    
    Monidroid::TaggedLog("Server", "Server stopped");
}

void Server::serverMain() {
    while (m_started) {
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
    m_acceptor.async_accept([this](const boost::system::error_code &ec, ip::tcp::socket clientSocket) {
        if (!ec) {
            auto client = std::make_shared<Client>(std::move(clientSocket));
            
            // TODO: make list of clients
            client->m_communicationThread = std::thread([client]() {
                communicationMain(client);
            });
            client->m_communicationThread.detach();

            serverMainAsync();
        } else {
            Monidroid::TaggedLog("Server", "Ended accepting connections (message: {})", ec.message());
        }
    });
}

void Server::communicationMain(std::shared_ptr<Client> client) {
    Monidroid::DefaultLog("New client connected");

    // 1. Identify device
    client->identifyClient();

    // 2. Connect monitor
    int code = client->connectMonitor();

    // 3. Send frames
    code = client->sendFrames();
    
    // 4. Disconnect monitor
    code = client->disconnectMonitor();

    // 5. Finalize
    client->finalize();

    Monidroid::DefaultLog("Client {} disconnected", client->m_modelName);
}
