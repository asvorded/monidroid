#ifdef LIBUS_NO_SSL
constexpr bool SSL = false;
#else
constexpr bool SSL = true;
#endif

#include <iostream>
#include <thread>
#include <memory>
#include <csignal>
#include <unordered_map>

#include <boost/program_options.hpp>

#include <gst/gst.h>

#include <App.h>

#include <nlohmann/json.hpp>

#include "monidroid.h"
#include "monidroid/logger.h"
#include "echoserver.h"
#include "usbserver.h"
#include "server.h"

namespace po = boost::program_options;
using namespace uWS;
using namespace nlohmann;

struct ControlContext { };

std::unique_ptr<uWS::App> g_app;
uWS::Loop *g_loop = nullptr;
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

void setupWebSocketControl(uWS::App *app) {
    static auto handlers = std::unordered_map {
        std::pair(std::string("/config/shutdown"), []() { shutdown(); }),
    };

    app->ws<ControlContext>("/", {
        .open = [](WebSocket<SSL, true, ControlContext> *ws) {
            Monidroid::DefaultLog("Panel connected");

            ws->subscribe("main");
        },

        .message = [](WebSocket<SSL, true, ControlContext> *ws, std::string_view message, uWS::OpCode opCode) {
            json obj = json::parse(message, nullptr, false);
            if (obj.is_discarded()) {
                Monidroid::DefaultLog("Received unsupported message: \"{}\"", message);
                return;
            } else if (!obj.contains("message")) {
                Monidroid::DefaultLog("Property \"message\" not found in message {}", message);
                return;
            }

            auto &msg = obj["message"].get_ref<const json::string_t&>();
            if (handlers.contains(msg)) {
                handlers[msg]();
            } else {
                Monidroid::DefaultLog("Unknown message \"{}\"", msg);
            }
        },

        .close = [](auto *ws, int code, std::string_view message) {
            Monidroid::DefaultLog("Panel disconnected");
        }
    });
}

void setupControl(uWS::App *app) {

    // GET server configuration
    app->get("/config/all", [](HttpResponse<SSL> *res, HttpRequest *req) {
        json obj;
        if (g_server) {
            ServerInfo info = g_server->serverInfo();
            obj = {
                { "version", MD_SERVER_VERSION },
                { "enabled", g_server->running() },
                { "computerName", info.hostname },
                { "addresses", info.addrs }
            };
        } else {
            obj["error"] = "Server is not running";
            res->writeStatus("400 Bad Request");
        }

        res->writeHeader("Content-Type", "application/json");
        res->end(obj.dump());
    });

    // GET current clients
    app->get("/clients/all", [](HttpResponse<SSL> *res, HttpRequest *req) {
        json obj;
        if (g_server) {
            obj["clients"] = json::array();
            for (const auto& c : g_server->clients()) {
                obj["clients"].push_back(getClientJson(*c.get()));
            }
        } else {
            obj["error"] = "Server is not running";
            res->writeStatus("400 Bad Request");
        }

        res->writeHeader("Content-Type", "application/json");
        res->end(obj.dump());
    });

    // POST server state
    app->post("/config/serverState", [](HttpResponse<SSL> *res, HttpRequest *req) {
        res->onData([res, buffer = std::make_unique<std::string>()](std::string_view chunk, bool isFin) mutable {
            buffer->append(chunk);
            if (!isFin) {
                return;
            }

            json obj = json::parse(*buffer);
            bool enable = obj["enable"].get<bool>();
            if (enable) {
                startServers();
            } else {
                stopServers();
            }

            res->writeHeader("Content-Type", "application/json");
            res->end(obj.dump());
        });

        // We only rely on RAII in unique_ptr (from examples)
        res->onAborted([]() {});
    });

    // WebSocket setup
    setupWebSocketControl(app);
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

    g_app = std::make_unique<uWS::App>();
    g_loop = uWS::Loop::get();

    g_notifier = {
        .onClientConnected = [app = g_app.get()](auto ctx) {
            g_loop->defer([app, ctx]() {
                json obj = makeMessage(Monidroid::Control::CONNECTED_EVENT, "client", getClientJson(*ctx));
                app->publish("main", obj.dump(), uWS::OpCode::TEXT);
            });
        },
        .onClientDisconnected = [app = g_app.get()](auto ctx) {
            g_loop->defer([app, ctx]() {
                json obj = makeMessage(Monidroid::Control::DISCONNECTED_EVENT, "id", ctx->id);
                app->publish("main", obj.dump(), uWS::OpCode::TEXT);
            });
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
    g_app->listen(Monidroid::Control::PORT, [](us_listen_socket_t *s) {
        if (s) {
            Monidroid::DefaultLog("Control panel interface is listening on port {}", Monidroid::Control::PORT);
        } else {
            throw std::runtime_error("Failed to open control panel interface");
        }
    });

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