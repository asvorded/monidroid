#include "native.h"

#include <stdexcept>

#include <windows.h>
#include <shlobj.h>

void checkElevation() noexcept(false) {
    bool isAdmin = false;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size;
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
    }
    if (token) CloseHandle(token);

    if (!isAdmin) {
        throw std::runtime_error("Server must be run with administrator or service privileges");
    }
}