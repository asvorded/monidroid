#include <iostream>
#include <thread>
#include <memory>
#include <csignal>

#include <boost/program_options.hpp>

#include <gst/gst.h>

#include <monidroid.h>

#include "monidroid/logger.h"
#include "echoserver.h"
#include "usbserver.h"
#include "server.h"
#include "native.h"

namespace po = boost::program_options;

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

boost::asio::io_context context;

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
        Monidroid::DefaultLog("Starting as console applicaion...");
    }
    bool hideSerials = !vm.contains("show-serials");
    bool throwIfUsbFailed = vm.contains("force-usb");

    gst_init(&argc, &argv);
    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    // Health checks
    checkElevation();
    videoHealthCheck();
    
    EchoServer echoServer(context);
    echoServer.start();
    
    Server server(context);

    std::unique_ptr<UsbServer> usbServer;
    if (throwIfUsbFailed) {
        usbServer.reset(new UsbServer(hideSerials));
    } else {
        try {
            usbServer.reset(new UsbServer(hideSerials));
        } catch (const std::runtime_error &e) {
            Monidroid::DefaultLog("{}", e.what());
            Monidroid::DefaultLog("USB server has failed to start, server will not support USB connections");
        }
    }
    
    std::signal(SIGINT, [](int signal) {
        Monidroid::DefaultLog("Stop requested, shutting down the server...");

        context.stop();
    });

    // Main loop, main thread is running until this context is stopped
    context.run();
    
    return 0;
} catch (const std::exception& e) {
    Monidroid::DefaultLog("{}", e.what());
    Monidroid::DefaultLog("Exiting due to critical error");

    return -1;
}
