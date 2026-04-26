#define BOOST_TEST_MODULE USB connection tests
#define DEBUG

#include <boost/test/included/unit_test.hpp>
#include <boost/asio.hpp>
#include <boost/process.hpp>

#include <libusb.h>

#include "monidroid/logger.h"
#include "monidroid/protocol.h"
#include "monidroid/debug.h"

using namespace boost;

asio::io_context io_context;

libusb_device *dev = nullptr;

int hotplugCallback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *data) {
    ::dev = libusb_ref_device(dev);
    MD_TRACE_LINE();

    return 0;
}

void handleDevice(libusb_device *dev) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    libusb_device_handle *handle = NULL;
    int rc = libusb_open(dev, &handle);
    if (LIBUSB_SUCCESS != rc) {
        return;
    }
    
    libusb_device_descriptor desc;
    libusb_config_descriptor *cfg = nullptr;
    libusb_get_device_descriptor(dev, &desc);
    libusb_get_config_descriptor(dev, 0, &cfg);

    for (int i = 0; i < cfg->bNumInterfaces; ++i) {
        libusb_interface interface = cfg->interface[i];
        for (int idx = 0; idx < interface.num_altsetting; ++idx) {
            const auto &$if = interface.altsetting[i];
            if ($if.bInterfaceClass == Monidroid::ADB_IF_CLASS
                && $if.bInterfaceSubClass == Monidroid::ADB_IF_SUBCLASS
                && $if.bInterfaceProtocol == Monidroid::ADB_IF_PROTOCOL)
            {
                char name[256];
                char serial[128];
            
                libusb_get_string_descriptor_ascii(handle, desc.iProduct, (unsigned char*)name, sizeof(name));
                libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char*)serial, sizeof(serial));
                Monidroid::DefaultLog("Android device detected: {}, serial number: {}", name, serial);
                
                process::popen proc(io_context, "/home/asvorded/Android/Sdk/platform-tools/adb", {
                    "-s", serial,
                    "reverse",
                    "tcp:0",
                    "tcp:" + std::to_string(Monidroid::ADB_PORT)
                });
                
                if (proc.wait() == 0) {
                    std::string port("xxxxx");
                    system::error_code ec;
                    asio::read(proc, asio::buffer(port), ec);
                    Monidroid::DefaultLog("Got number port {}", port);
                }
            }
        }
    }

    libusb_free_config_descriptor(cfg);
    
    libusb_close(handle);
}

BOOST_AUTO_TEST_CASE(UsbDetectionTest) {
    libusb_init_context(nullptr, nullptr, 0);

    BOOST_ASSERT(libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG));

    int count = 0;

    libusb_hotplug_callback_handle handle;
    int code = libusb_hotplug_register_callback(
        nullptr, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0,
        LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
        hotplugCallback, static_cast<void*>(&count), &handle
    );

    BOOST_ASSERT(code == LIBUSB_SUCCESS);

    timeval tv { .tv_sec = 10 };
    while (count < 2) {
        libusb_handle_events_timeout(NULL, &tv);
        if (dev) {
            MD_TRACE_LINE();
            handleDevice(dev);
            libusb_unref_device(dev);
            ++count;
            dev = nullptr;
        }
    }
 
    libusb_hotplug_deregister_callback(NULL, handle);
    libusb_exit(NULL);
}

BOOST_AUTO_TEST_CASE(StoiTest) {
    BOOST_ASSERT(std::stoi(std::string("123xx")) == 123);
}
