#include "native.h"

#include <evdi_lib.h>

#include <stdexcept>
#include <fstream>

#include "monidroid/logger.h"

static constexpr auto EVDI_HEALTH_CHECK_PATH = "/sys/devices/evdi/version";
static constexpr auto HEALTH_CHECK_TAG = "EVDI health check";

struct MonitorContext {

};

void MonitorContextDeleter::operator()(MonitorContext* p) const {
    delete p;
}

struct AdapterContext {

};

void videoHealthCheck() noexcept(false) {
    std::ifstream vf(EVDI_HEALTH_CHECK_PATH);
    if (!vf) {
        // Load maybe?
        throw std::runtime_error("EVDI module not loaded! Please load EVDI by entering \"sudo modprobe evdi\".");
    }
    std::string ver;
    vf >> ver;
    Monidroid::TaggedLog(HEALTH_CHECK_TAG, "Found EVDI version {}", ver);
}

Adapter openAdapter() {
    return Adapter(new AdapterContext());
}

Monitor adapterConnectMonitor(const Adapter &adapter, const std::string &modelName, const MonitorMode &info) {
    return Monitor();
}

MDStatus monitorRequestFrame(const Monitor &monitor) {
    return MDStatus::MonitorOff;
}

MonitorMode monitorRequestMode(const Monitor &monitor, bool cached) {
    return MonitorMode();
}

void monitorMapCurrent(const Monitor &self, FrameMapInfo &mapInfo) {
}

void monitorUnmap(const Monitor &self) {
}

void monitorDisconnect(Monitor &monitor) {
    monitor.reset();
}
