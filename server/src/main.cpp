#include <errno.h>

#include <iostream>
#include <thread>

#include <gst/gst.h>

#include <monidroid.h>

#include "echoserver.h"
#include "logger.h"
#include "server.h"
#ifdef __linux__
#include "linux/utils.h"
#else
#include "win32/utils.h"
#endif

static void usage() {
    std::cout <<
#if defined(__linux__)
        "Monidroid Linux server version " MONIDROID_SERVICE_VERSION "\n"
        "Options:\n"
        "--help            Print help" "\n"
        "--no-service      Start as a console application" "\n"
#elif defined(_WIN32)
        "Monidroid Windows server version " MONIDROID_SERVICE_VERSION "\n"
        "Options:\n"
        "--help            Print help" "\n"
        "--install         Install as a Windows service" "\n"
        "--uninstall       Uninstall Windows service" "\n"
        "--no-service      Start as a console application" "\n"
#endif
    ;
}

int main(int argc, char *argv[]) {
    bool runAsService = true;

    if (argc == 2) {
		std::string command(argv[1]);
		if (command == "--help") {
            usage();
            return 0;
        } else if (command == "--no-service") {
			runAsService = false;
            Monidroid::DefaultLog("Starting as console applicaion...");
		} else {
			std::cout << "Unknown option. Use --help to get available options." << '\n';
            return EINVAL;
		}
	} else if (argc != 1) {
        std::cout << "Invalid usage. Use --help to get more info." << '\n';
        return EINVAL;
    }

    gst_init(&argc, &argv);
    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);

    Monidroid::DefaultLog("GStreamer version: {}.{}.{}.{}", major, minor, micro, nano);

#ifdef __linux__
    // EVDI health check
    evdiHealthCheck();
#else
    iddcxHealthCheck();
#endif

    boost::asio::io_context context;
    
    EchoServer echoServer(context);
    echoServer.start();
    
    Server server(context);
    server.start();
    
    std::jthread t([&]() {
        context.run();
    });
    
    if (!runAsService) {
        std::cout << "Started. Accepting connections...\n";
        
        std::cout << "Press ENTER to stop.\n";
        std::cin.get();
        
        std::cout << "Stopping...\n";
    }
    
    context.stop();
    return 0;
}
