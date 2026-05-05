#define BOOST_TEST_MODULE USB connection tests

#include <boost/test/included/unit_test.hpp>

#include "../server/usbserver.h"

struct GlobalConfig {
    GlobalConfig() {
        boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
        BOOST_TEST_MESSAGE("WARNING: Ensure that no device is connected");
    }
};

BOOST_GLOBAL_FIXTURE(GlobalConfig);

boost::asio::io_context ctx;

BOOST_AUTO_TEST_CASE(StartStopTest) {
    UsbServer server(ctx);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    BOOST_CHECK(server.running());
}

BOOST_AUTO_TEST_CASE(HotplugTest) {
    UsbServer server(ctx);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    BOOST_CHECK_EQUAL(server.foundDevices().size(), 0);

    BOOST_TEST_MESSAGE("\nPlug the device now. Waiting 3 seconds...\n");
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    BOOST_CHECK_EQUAL(server.foundDevices().size(), 1);
}

BOOST_AUTO_TEST_CASE(EnumerateOneDeviceTest) {
    UsbServer server(ctx);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    BOOST_CHECK_EQUAL(server.foundDevices().size(), 1);
}
