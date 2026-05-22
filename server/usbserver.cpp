#include "usbserver.h"

#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/dll.hpp>
#ifdef _WIN32
#include <initguid.h>
#include <devguid.h>
#include <devpkey.h>
#include <SetupAPI.h>
#endif

#include "monidroid/protocol.h"
#include "monidroid/logger.h"
#include "monidroid/debug.h"

UsbServer::UsbServer(asio::io_context &ctx, bool hideSerials)
  : m_hideSerials(hideSerials),
    m_adbPath(process::environment::find_executable(ADB_PATH)),
    m_io(ctx)
{
    if (m_adbPath.empty()) {
        m_adbPath = filesystem::absolute(ADB_PATH, dll::program_location().parent_path());
        if (!filesystem::exists(m_adbPath)) {
            throw std::runtime_error("ADB executable was not found");
        }
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
    m_thread = std::thread(std::bind(&UsbServer::usbMain, this));
    Monidroid::TaggedLog(TAG, "USB server started");
}

UsbServer::~UsbServer() {
    m_running = false;
    m_thread.join();
    
    libusb_hotplug_deregister_callback(NULL, m_callback);
    libusb_exit(NULL);
    
    Monidroid::TaggedLog(TAG, "USB server stopped");
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
                    } else if (alt.bInterfaceClass == 0xFFu) {
                        Monidroid::TaggedLog(TAG, "Device with vendor-specific interface class detected, will try to start ADB server");
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

#ifdef _WIN32
BOOL GetSerialNumberByVidPid(DWORD dwVid, DWORD dwPid, char* serial, const size_t bufSize) {
    HDEVINFO hDevInfo;
    SP_DEVINFO_DATA devInfoData;
    BOOL result = FALSE;

    hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_USB, 0, 0, DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) return FALSE;

    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        WCHAR hardwareId[128] { };
        DEVPROPTYPE devPropType = 0;
        if (SetupDiGetDevicePropertyW(hDevInfo, &devInfoData, &DEVPKEY_Device_Parent, &devPropType,
            (PBYTE)hardwareId, sizeof(hardwareId), nullptr, 0)) {
            DWORD vid, pid;
            WCHAR wserial[128] { };
            int r = swscanf(hardwareId, L"USB\\VID_%X&PID_%X\\%128s", &vid, &pid, wserial);
            if (r == 3) {
                if (vid == dwVid && pid == dwPid) {
                    WideCharToMultiByte(CP_ACP, 0, wserial, -1, serial, bufSize, nullptr, nullptr);
                    result = TRUE;
                    break;
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return result;
}
#endif // _WIN32


void UsbServer::handleAdbDevice(libusb_device *dev) {
    // Wait for authorization
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    libusb_device_handle *handle = NULL;
    int rc = libusb_open(dev, &handle);
    if (LIBUSB_SUCCESS != rc) {
        Monidroid::TaggedLog(TAG, "Cannot open Android device, error code {}", rc);
#ifdef _WIN32
        libusb_device_descriptor desc;
        libusb_get_device_descriptor(dev, &desc);

        char serialBuf[128] {};

        if (GetSerialNumberByVidPid(desc.idVendor, desc.idProduct, serialBuf, sizeof(serialBuf))) {
            // Wait for device become available to adb
            std::this_thread::sleep_for(std::chrono::seconds(1));

            std::string serial = serialBuf;
            std::string serialToShow = m_hideSerials ? std::string(serial.size(), '*') : serial;
            Monidroid::TaggedLog(TAG, "Found Android device {} from Setup API", serialToShow);

            startListening(serial);
        } else {
            Monidroid::TaggedLog(TAG, "Setup API: Cannot open Android device");
        }
#endif
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

    libusb_close(handle);
#ifdef _WIN32
    // Wait for device become available to adb after libusb_close()
    // USB Server works like casino because WinUSB does not support multiple concurrent applications
    // https://github.com/libusb/libusb/wiki/Windows#known-restrictions
    std::this_thread::sleep_for(std::chrono::seconds(1));
#endif

    startListening(serial);
}

void UsbServer::startListening(const std::string &serial) {
    process::popen proc(m_io, m_adbPath.c_str(), {
        "-s", serial,
        "reverse",
        "tcp:" + std::to_string(Monidroid::PROTOCOL_PORT),
        "tcp:" + std::to_string(Monidroid::PROTOCOL_PORT),
    });
    
    if (proc.wait() == 0) {
        int port = Monidroid::PROTOCOL_PORT;

        auto ctx = std::shared_ptr<UsbClientContext>(new UsbClientContext {
            .serial = serial,
            .port = port
        });
        
        {
            std::lock_guard g(lock);
            m_clients.insert(ctx);
        }
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
