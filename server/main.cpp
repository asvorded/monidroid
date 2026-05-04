#include <iostream>
#include <thread>
#include <memory>
#include <csignal>
#include <unordered_map>

#include <boost/program_options.hpp>
#include <gst/gst.h>
#include <nlohmann/json.hpp>

#include "monidroid.h"
#include "monidroid/logger.h"
#include "echoserver.h"
#include "usbserver.h"
#include "server.h"
#include "control.h"

namespace po = boost::program_options;
using namespace uWS;
using namespace nlohmann;

std::unique_ptr<MDApp> g_app;
Notifier g_notifier;

boost::asio::io_context context;

std::unique_ptr<Server> g_server;
std::unique_ptr<EchoServer> g_echoServer;
std::unique_ptr<UsbServer> g_usbServer;

bool g_hideSerials;
bool g_throwIfUsbFailed;

static void usage(const po::options_description &desc) {
    std::cout <<
#if defined(__linux__)
        "Monidroid Linux server version " MD_SERVER_VERSION "\n"
#elif defined(_WIN32)
        "Monidroid Windows server version " MD_SERVER_VERSION "\n"
#endif
        "Usage: " MD_SERVER_EXE_NAME " [options...]" "\n"
        << desc << "\n"
    ;
}

static void version() {
    std::cout << MD_SERVER_VERSION << "\n";
}

static void startServers() {
    if (!g_server || !g_server->running()) {
        g_server = std::make_unique<Server>(context, g_notifier);
    }
    if (!g_echoServer || !g_echoServer->running()) {
        g_echoServer = std::make_unique<EchoServer>(context);
    }
    if (!g_usbServer || !g_usbServer->running()) {
        if (!g_throwIfUsbFailed) {
            g_usbServer = std::make_unique<UsbServer>(g_hideSerials);
        }
    }
}

static void stopServers() {
    g_server.reset();
    g_echoServer.reset();
    g_usbServer.reset();
}

static void shutdown() {
    context.stop();
    stopServers();

    g_app->close();
}

json getClientJson(const ClientContext& ctx) {
    return {
        { "id", std::to_string(ctx.id) },
        { "address", ctx.address.address().to_string() },
        { "connectionType", ctx.client->isUsb() ? "usb" : "wifi" },
        { "name", ctx.client->modelName() },
        { "connectedAt", ctx.connectedAt.time_since_epoch().count() }
    };
}

json makeMessage(std::string_view message, std::string_view objKey, const json &obj) {
    json j;
    j["message"] = message;
    j[objKey] = obj;
    return j;
}

void setupControl(MDApp *app) {

    // Get server configuration
    app->onRequest(Monidroid::Control::GET_CONFIG, [](const json &data, std::string &error) {
        std::vector<std::string> addrs;
        asio::ip::tcp::resolver resolver(context);
        std::string hostname = asio::ip::host_name();
        for (const auto &it : resolver.resolve(hostname, "")) {
            auto addr = it.endpoint().address();
            if (addr.is_v4()) {
                addrs.push_back(addr.to_string());
            }
        }
        json obj;
        obj["version"] = MD_SERVER_VERSION;
        obj["enabled"] = g_server->running();
        obj["computerName"] = hostname;
        obj["addresses"] = addrs;

        return obj;
    });

    // Get current clients
    app->onRequest(Monidroid::Control::GET_CLIENTS, [](const json &data, std::string &error) {
        json obj;
        obj["clients"] = json::array();
        if (g_server) {
            for (const auto& c : g_server->clients()) {
                obj["clients"].push_back(getClientJson(*c.get()));
            }
        }

        return obj;
    });

    // Set server state
    app->onRequest(Monidroid::Control::SET_STATE, [](const json &data, std::string &error) {
        if (!data.contains("enable")) {
            error = "Incorrect usage";
            return json();
        }
        
        bool enable = data["enable"].get<bool>();
        if (enable) {
            startServers();
        } else {
            stopServers();
        }

        json obj;
        obj["enabled"] = enable;

        return obj;
    });

    // Shutdown
    app->onEvent(Monidroid::Control::SHUTDOWN, [](const json &) {
        shutdown();
    });
}

int main(int argc, char *argv[]) try {
    po::options_description desc("Options");
    desc.add_options()
        ("version", "Print version")
        ("help", "Print help")
        ("terminal", "Start as a console application")
        ("show-serials", "Show mobile devices' serial numbers")
        ("force-usb", "Do not start server if USB server cannot be started")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("version")) {
        version();
        return 0;
    } else if (vm.contains("help")) {
        usage(desc);
        return 0;
    }

    if (vm.contains("terminal")) {
        Monidroid::DefaultLog("Starting as console application...");
    }
    g_hideSerials = !vm.contains("show-serials");
    g_throwIfUsbFailed = vm.contains("force-usb");

    gst_init(&argc, &argv);

    // Health checks
    // checkElevation();
    // videoHealthCheck();

    g_app = std::make_unique<MDApp>();

    g_notifier = {
        .onClientConnected = [](auto ctx) {
            g_app->emit(Monidroid::Control::CONNECTED_EVENT, "client", getClientJson(*ctx));
        },
        .onClientDisconnected = [](auto ctx) {
            g_app->emit(Monidroid::Control::DISCONNECTED_EVENT, "id", std::to_string(ctx->id));
        }
    };

    // Start server
    g_server = std::make_unique<Server>(context, g_notifier);

    // Start ECHO server
    g_echoServer = std::make_unique<EchoServer>(context);

    // Start USB server
    if (g_throwIfUsbFailed) {
        g_usbServer = std::make_unique<UsbServer>(g_hideSerials);
    } else {
        try {
            g_usbServer = std::make_unique<UsbServer>(g_hideSerials);
        } catch (const std::runtime_error &e) {
            Monidroid::DefaultLog("{}", e.what());
            Monidroid::DefaultLog("USB server has failed to start, server will not support USB connections");
        }
    }

    // Start control server
    setupControl(g_app.get());

    // Register exit signal
    std::signal(SIGINT, [](int) {
        Monidroid::DefaultLog("Stop requested, shutting down the server...");

        shutdown();
    });

    // Main loop, main thread is running until this context is stopped
    Monidroid::DefaultLog("Server ready");
    std::jthread asioLoop([]() {
        auto work = asio::make_work_guard(context);
        context.run();
    });
    g_app->run();
    
    // Shit library ;)
    // uWS::App must be destroyed in the thread it was constructed
    // and BEFORE going out of main() scope
    g_app.reset();

    return 0;
} catch (const std::exception& e) {
    Monidroid::DefaultLog("{}", e.what());
    Monidroid::DefaultLog("Exiting due to critical error");

    return -1;
}