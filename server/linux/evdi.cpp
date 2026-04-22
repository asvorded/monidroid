#include "native.h"

#include <stdarg.h>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <string>

#include <sys/poll.h>
#include <sys/file.h>
#include <drm/drm_mode.h>

#include <evdi_lib.h>
#include <libevdev/libevdev-uinput.h>

#include "monidroid.h"
#include "monidroid/debug.h"
#include "monidroid/logger.h"

using namespace Monidroid;

static constexpr auto EVDI_HEALTH_CHECK_PATH = "/sys/devices/evdi/version";
static constexpr auto HEALTH_CHECK_TAG = "EVDI health check";
static constexpr auto ADAPTER_TAG = "EVDI";

static constexpr int DEV_NO_THRESHOLD = 10000;
static constexpr int MAX_FRAME_WAIT = 5'000;

struct MonitorContext {
    static void dpmsHandler(int dpms_mode, void *user_data);
    static void modeHandler(evdi_mode mode, void *user_data);
    static void updateHandler(int buffer_to_be_updated, void *user_data);

    MonitorContext(
        evdi_handle handle, int devIndex,
        MonitorMode preferred, const std::string &modelName
    );

    evdi_event_context ctx {
        .dpms_handler = dpmsHandler,
        .mode_changed_handler = modeHandler,
        .update_ready_handler = updateHandler,
        .user_data = this,
    };

    evdi_handle handle;
    int devIndex;

    libevdev_uinput *uinput = nullptr;

    MonitorMode preferred;
    MonitorMode current;
    bool enabled = false;

    std::string modelName;

    std::vector<ColorType> frameBuffer;
    evdi_buffer frameBufferInfo { };
    
    int rectsCount = 0;
    std::array<evdi_rect, 16> rects { };
};

MonitorContext::MonitorContext(
    evdi_handle handle, int devIndex,
    MonitorMode preferred, const std::string &modelName
) : handle(handle),
    devIndex(devIndex),
    preferred(preferred),
    current(preferred),
    modelName(modelName)
{ }

void MonitorContextDeleter::operator()(MonitorContext *p) const {
    delete p;
}

struct AdapterContext {
    int lastNumber = -1;
};

void evdiLog(void *user_data, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    char str[512];

    vsnprintf(str, 512, fmt, va);
    Monidroid::TaggedLog(ADAPTER_TAG, "{}", str);

    va_end(va);
}

static void allocFrameBuffer(MonitorContext *self, int width, int height) {
    self->frameBuffer.reserve(width * height);

    self->frameBufferInfo = {
        .id = 0,
        .buffer = self->frameBuffer.data(),
        .width = width,
        .height = height,
        .stride = width * (int)sizeof(ColorType),
        // .rects = rects.data(),       // rects and rect_count are not used 
        // .rect_count = rects.size()   // in current libevdi implementation
    };
    evdi_register_buffer(self->handle, self->frameBufferInfo);
}

void MonitorContext::dpmsHandler(int dpms_mode, void *user_data) {
    MonitorContext* client = static_cast<MonitorContext*>(user_data);
    
    client->enabled = dpms_mode == DRM_MODE_DPMS_ON;
}

void MonitorContext::modeHandler(evdi_mode mode, void *user_data) {
    MonitorContext* client = static_cast<MonitorContext*>(user_data);

    Monidroid::TaggedLog(client->modelName, "Mode change: {}x{}@{} -> {}x{}@{}", 
        client->current.width, client->current.height, client->current.refreshRate,
        mode.width, mode.height, mode.refresh_rate
    );

    client->current = {
        .width = (u32)mode.width,
        .height = (u32)mode.height,
        .refreshRate = (u32)mode.refresh_rate
    };

    evdi_unregister_buffer(client->handle, client->frameBufferInfo.id);

    allocFrameBuffer(client, mode.width, mode.height);
}

void MonitorContext::updateHandler(int buffer_to_be_updated, void *user_data) {
    MonitorContext* client = static_cast<MonitorContext*>(user_data);
    
    // If there were no DPMS events, monitor is enabled
    if (client->enabled) {
        evdi_grab_pixels(client->handle, client->rects.begin(), &client->rectsCount);
    } else {
        client->rectsCount = 0;
    }
}


void videoHealthCheck() noexcept(false) {
    std::ifstream vf(EVDI_HEALTH_CHECK_PATH);
    std::string ver;
    if (vf) {
        vf >> ver;
    } else {
        int status = system("modprobe evdi");
        if (status == 0) {
            Monidroid::TaggedLog(HEALTH_CHECK_TAG, "EVDI module load attempt succeded");
            std::ifstream vf2(EVDI_HEALTH_CHECK_PATH);
            vf2 >> ver;
        } else {
            Monidroid::TaggedLog(HEALTH_CHECK_TAG, "EVDI module load attempt failed");
            throw std::runtime_error("Unable to continue because EVDI module cannot be loaded!");
        }
    }
    Monidroid::TaggedLog(HEALTH_CHECK_TAG, "Found EVDI version {}", ver);
}

Adapter openAdapter() {
    evdi_logging logger {
        .function = evdiLog,
        .user_data = nullptr
    };
    evdi_set_logging(logger);

    return Adapter(new AdapterContext());
}

Monitor adapterConnectMonitor(const Adapter &self, const std::string &modelName, const MonitorMode &info) {
    evdi_add_device();

    int devNumber = self->lastNumber + 1;
    while (evdi_check_device(devNumber) != AVAILABLE && devNumber < DEV_NO_THRESHOLD) devNumber++;
    if (devNumber == DEV_NO_THRESHOLD) {
        throw new std::runtime_error("[BUG] Device numbers issued inconsistently");
    }

    // Prepare EDID
    Monidroid::EDID edid = Monidroid::CUSTOM_EDID;
    edid.setDefaultMode(info.width, info.height, info.refreshRate);
    edid.commit();

    // Connect monitor
    evdi_handle handle = evdi_open(devNumber);
    evdi_connect2(handle,
        reinterpret_cast<const unsigned char *>(&edid), sizeof(edid),
        info.width * info.height,
        edid.dataBlocks[0].timing.pixel_clock * 10000
    );
    self->lastNumber = devNumber;

    Monitor monitor = Monitor(new MonitorContext(handle, devNumber, info, modelName));

    allocFrameBuffer(monitor.get(), info.width, info.height);

    std::string uinputName = "Monidroid Input (" + modelName + ")";
    // Init input
    libevdev *dev = libevdev_new();
    libevdev_set_name(dev, uinputName.c_str());
    libevdev_enable_event_type(dev, EV_SYN);
    libevdev_enable_event_type(dev, EV_REL);
    libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
    libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);

    libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &monitor->uinput);

    libevdev_free(dev);
    return monitor;
}

FrameStatus monitorRequestFrame(const Monitor &self) {
    bool ready = evdi_request_update(self->handle, self->frameBufferInfo.id);
    if (ready) {
        evdi_grab_pixels(self->handle, self->rects.begin(), &self->rectsCount);
        if (self->rectsCount == 0) {
            // mode changed according to current implementation
            return FrameStatus::ModeChanged;
        }
        self->enabled = true;
    } else {
        // DANGER: current definition of struct evdi_device_context is {
        //     int fd;
        //     int bufferToUpdate;
        //     ...
        // };
        std::array<pollfd, 1> polls = {
            pollfd { .fd = *reinterpret_cast<int*>(self->handle), .events = POLLIN }
        };
        int result = poll(polls.begin(), polls.size(), MAX_FRAME_WAIT);
        if (result < 0) {
            Monidroid::TaggedLog(self->modelName, "Failed to request frame, poll() failed with errno {}", errno);
            return FrameStatus::Error;
        } else if (result == 0) {
            if (self->enabled) {
                return FrameStatus::NoUpdates;
            } else {
                return FrameStatus::MonitorOff;
            }
        } else if (result > 0) {
            evdi_handle_events(self->handle, &self->ctx);
            if (self->enabled) {
                if (self->rectsCount == 0) {
                    // mode changed according to current implementation
                    return FrameStatus::ModeChanged;
                }
            } else {
                return FrameStatus::MonitorOff;
            }
        } else {
        }
    }

    return FrameStatus::FrameReady;
}

MonitorMode monitorRequestMode(const Monitor &self, bool cached) {
    return self->current;
}

void monitorSendInput(const Monitor &self, int dx, int dy) {
    libevdev_uinput_write_event(self->uinput, EV_REL, REL_X, dx);
    libevdev_uinput_write_event(self->uinput, EV_REL, REL_Y, dy);
    libevdev_uinput_write_event(self->uinput, EV_SYN, SYN_REPORT, 0);
}

void monitorMapCurrent(const Monitor &self, FrameMapInfo &mapInfo) {
    if (!self->enabled || self->rectsCount == 0) {
        mapInfo = { .data = nullptr };
    } else {
        mapInfo = {
            .data = (ColorType*)self->frameBufferInfo.buffer,
            .width = (u32)self->frameBufferInfo.width,
            .height = (u32)self->frameBufferInfo.height,
            .stride = (u32)self->frameBufferInfo.stride
        };
    }
}

void monitorUnmap(const Monitor &self) {
    // nothing here...
}

void monitorDisconnect(Monitor &self) {
    // Disconnect monitor
    evdi_close(self->handle);
    std::string path = "/dev/dri/card";
    path += std::to_string(self->devIndex);

    if (remove(path.c_str()) != 0) {
        Monidroid::TaggedLog(ADAPTER_TAG, "Card {} removal failed with code {}, ignoring", path, errno);
    }

    // Destroy input
    if (self->uinput) {
        libevdev_uinput_destroy(self->uinput);
    }

    self.reset();
}
