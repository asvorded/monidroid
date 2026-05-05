#pragma once

#include <queue>
#include <thread>
#include <set>
#include <mutex>
#include <memory>

#include <boost/process.hpp>

#include <libusb.h>

#include "native.h"

using namespace boost;

struct UsbClientContext {
    std::string serial;
    int port = 0;
};

class UsbServer {
    using ClientsSet = std::set<std::shared_ptr<UsbClientContext>>;
private:
    static constexpr auto TAG = "USB";
    static constexpr auto ADB_PATH = "adb";
    static constexpr timeval POLL_INTERVAL { .tv_sec = 1 };

    const bool m_hideSerials;
    const filesystem::path m_adbPath;

    asio::io_context &m_io;
    std::thread m_thread;
    bool m_running;
    libusb_hotplug_callback_handle m_callback;

    std::queue<libusb_device*> m_devices;
    ClientsSet m_clients;
    std::mutex lock;

    static int hotplugCallback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *data);
    void usbMain();

    bool isAdbDevice(libusb_device *dev, int& errc);
    void handleAdbDevice(libusb_device *dev);
    void startListening(const std::string &serial);
    void handleSaved();
        
public:
    UsbServer(asio::io_context &ctx, bool hideSerials = true);
    ~UsbServer();

    bool running() const;
    ClientsSet foundDevices();
};
