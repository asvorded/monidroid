#include "native.h"

#include <stdexcept>

#include <unistd.h>

void checkElevation() noexcept(false) {
    if (geteuid() != 0) {
        throw std::runtime_error("Server must be run as root");
    }
}