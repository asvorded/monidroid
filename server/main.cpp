#include <iostream>
#include <thread>
#include <csignal>

#include <gst/gst.h>

#include <monidroid.h>

#include "monidroid/logger.h"
#include "echoserver.h"
#include "server.h"
#include "native.h"

static void usage() {
    std::cout <<
#if defined(__linux__)
        "Monidroid Linux server version " MONIDROID_SERVICE_VERSION "\n"
        "Options:\n"
        "--help            Print help" "\n"
        "--terminal        Start as a console application" "\n"
#elif defined(_WIN32)
        "Monidroid Windows server version " MONIDROID_SERVICE_VERSION "\n"
        "Options:\n"
        "--help            Print help" "\n"
        "--install         Install as a Windows service" "\n"
        "--uninstall       Uninstall Windows service" "\n"
        "--terminal        Start as a console application" "\n"
#endif
    ;
}

boost::asio::io_context context;

int main(int argc, char *argv[]) try {
    if (argc == 2) {
		std::string command(argv[1]);
		if (command == "--help") {
            usage();
            return 0;
        } else if (command == "--terminal") {
            Monidroid::DefaultLog("Starting as console applicaion...");
		} else {
			std::cout << "Unknown option. Use --help to get available options." << '\n';
            return -1;
		}
	} else if (argc != 1) {
        std::cout << "Invalid usage. Use --help to get more info." << '\n';
        return -1;
    }

    gst_init(&argc, &argv);
    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    Monidroid::DefaultLog("GStreamer version: {}.{}.{}.{}", major, minor, micro, nano);

    // Video adapter health check
    videoHealthCheck();
    
    EchoServer echoServer(context);
    echoServer.start();
    
    Server server(context);
    
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
