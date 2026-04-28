#include "usbserver.h"

#include <stdexcept>

#include <boost/asio.hpp>

#include "monidroid/protocol.h"
#include "monidroid/logger.h"
#include "monidroid/debug.h"

using namespace boost::asio;

UsbServer::UsbServer(bool hideSerials)
  : m_hideSerials(hideSerials),
    m_adbPath(process::environment::find_executable(ADB_PATH))
{
    if (m_adbPath.empty()) {
        throw std::runtime_error("ADB executable was not found");
    }

    libusb_init_context(nullptr, nullptr, 0);

    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        libusb_exit(NULL);
        throw std::runtime_error("USB server cannot be started because the platform does not support hotplugging");
    }

    libusb_hotplug_register_callback(
        NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, LIBUSB_HOTPLUG_ENUMERATE,
        LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
        hotplugCallback, this, &m_callback
    );

    m_running = true;
    m_thread = std::thread([this]() { usbMain(); });
}

UsbServer::~UsbServer() {
    m_running = false;
    m_thread.join();

    libusb_hotplug_deregister_callback(NULL, m_callback);
    libusb_exit(NULL);
}

bool UsbServer::running() const {
    return m_running;
}

auto UsbServer::foundDevices() -> ClientsSet {
    std::lock_guard g(lock);
    return m_clients;
}

int UsbServer::hotplugCallback(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *data) {
    UsbServer *self = static_cast<UsbServer*>(data);
    int errc = LIBUSB_SUCCESS;

    if (self->isAdbDevice(dev, errc)) {
        auto ref = libusb_ref_device(dev);
        self->m_devices.push(ref);
    } else if (errc != LIBUSB_SUCCESS) {
        Monidroid::DefaultLog("Failed to check device, error code {}", errc);
    }

    return 0;
}

bool UsbServer::isAdbDevice(libusb_device *dev, int& errc) {
    bool result = false;
    
    libusb_device_descriptor desc;
    int rc = libusb_get_device_descriptor(dev, &desc);
    if (rc != LIBUSB_SUCCESS) {
        errc = rc;
        return false;
    }

    for (int c = 0; c < desc.bNumConfigurations; ++c) {
        libusb_config_descriptor *cfg = nullptr;
        rc = libusb_get_config_descriptor(dev, c, &cfg);
        
        if (rc == LIBUSB_SUCCESS) {
            for (int i = 0; i < cfg->bNumInterfaces; ++i) {
                libusb_interface interface = cfg->interface[i];
                for (int idx = 0; idx < interface.num_altsetting; ++idx) {
                    const auto &alt = interface.altsetting[i];
                    unsigned int ifId = (alt.bInterfaceClass << 16) | (alt.bInterfaceSubClass << 8) | (alt.bInterfaceProtocol);
                    if (ifId == Monidroid::ADB_IF_CLASS || ifId == Monidroid::ADB_MTP_IF_CLASS)
                    {
                        result = true;
                    }
                }
            }
                
            libusb_free_config_descriptor(cfg);
        } else {
            errc = rc;
        }
    }

    errc = LIBUSB_SUCCESS;
    return result;
}

void UsbServer::handleAdbDevice(libusb_device *dev) {
    // Wait for authorization
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    libusb_device_handle *handle = NULL;
    int rc = libusb_open(dev, &handle);
    if (LIBUSB_SUCCESS != rc) {
        Monidroid::TaggedLog(TAG, "Cannot open Android device, error code {}", rc);
        return;
    }
    
    libusb_device_descriptor desc;
    libusb_get_device_descriptor(dev, &desc);

    char nameBuf[256];
    char serialBuf[128];

    libusb_get_string_descriptor_ascii(handle, desc.iProduct, (unsigned char*)nameBuf, sizeof(nameBuf));
    libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, (unsigned char*)serialBuf, sizeof(serialBuf));
    std::string serial = serialBuf;
    std::string serialToShow = m_hideSerials ? std::string(serial.size(), '*') : serial;
    Monidroid::TaggedLog(TAG, "Android device detected: {}, serial number: {}", nameBuf, serialToShow);

    startListening(serial);
    
    libusb_close(handle);
}

void UsbServer::startListening(const std::string &serial) {
    // TODO: Maybe use main context?
    asio::io_context io_context;
    process::popen proc(io_context, m_adbPath, {
        "-s", serial,
        "reverse",
        "tcp:" + std::to_string(Monidroid::PROTOCOL_PORT),
        "tcp:" + std::to_string(Monidroid::PROTOCOL_PORT),
    });
    
    if (proc.wait() == 0) {
        // std::string p("xxxxx");
        // system::error_code ec;
        // asio::read(proc, asio::buffer(p), ec);
        // if (!ec) {
        // int port = std::stoi(p);
        int port = Monidroid::PROTOCOL_PORT;

        auto ctx = std::shared_ptr<UsbClientContext>(new UsbClientContext {
            .serial = serial,
            .port = port
        });
        
        {
            std::lock_guard g(lock);
            m_clients.insert(ctx);
        }
        // }
    }
}

void UsbServer::handleSaved() {
    while (m_devices.size() > 0) {
        libusb_device *ref = m_devices.front();
        handleAdbDevice(ref);
        m_devices.pop();
        libusb_unref_device(ref);
    }
}

void UsbServer::usbMain() {
    // Handle enumerated devices
    handleSaved();

    timeval tv = POLL_INTERVAL;
    while (m_running) {
        libusb_handle_events_timeout(NULL, &tv);
        handleSaved();
    }
}
