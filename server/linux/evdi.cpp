#include "native.h"

#include <evdi_lib.h>

#include <stdexcept>
#include <fstream>

#include "monidroid/logger.h"

static constexpr auto EVDI_HEALTH_CHECK_PATH = "/sys/devices/evdi/version";

void videoHealthCheck() noexcept(false) {
    std::ifstream vf(EVDI_HEALTH_CHECK_PATH);
    if (!vf) {
        // Load maybe?
        throw std::runtime_error("EVDI module not loaded! Please load EVDI by entering \"sudo modprobe evdi\".");
    }
    std::string ver;
    vf >> ver;
    Monidroid::DefaultLog("Found EVDI version {}", ver);
}
