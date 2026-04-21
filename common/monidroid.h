#pragma once

#define MONIDROID_SERVICE_VERSION       "0.1.0"

#if defined(_WIN32)

#define MD_OS_ID    "WIN"

#elif !defined(MD_OS_ID) && defined(__linux__)
/**
 * TODO: If you want server to support distro icon in client,
 * define MD_OS_ID (-DMD_OS_ID="<3-letter distro code>") while running CMake
 * 
 * Distro codes table:
 * --------------------------------------
 * | Distro               | Code        |
 * |----------------------|-------------|
 * | Ubuntu (GNOME)       |     UBU     |
 * | Kubuntu (KDE)        |     KBU     |
 * | Xubuntu (XFCE)       |     XBU     |
 * | Lubuntu (LXQt)       |     LBU     |
 * | Debian               |     DEB     |
 * | Zorin OS             |     ZOR     |
 * | Pop!_OS              |     POP     |
 * | Arch Linux           |     ARC     |
 * | CachyOS              |     CHY     |
 * | Bazzite              |     BAZ     |
 * | Manjaro              |     MAJ     |
 * | Fedora               |     FED     |
 * --------------------------------------
 */
#define MD_OS_ID    "GNU"

#endif

#define MD_CLASS_PTR_ONLY(Type) \
    Type(const Type&) = delete; \
    Type& operator=(const Type&) = delete;

#define MD_CLASS_NON_COPYABLE(Type) MD_CLASS_PTR_ONLY(Type); \
    Type(Type&&); \
    Type& operator=(Type&&);

namespace Monidroid {
    inline constexpr auto MAX_MONITORS_SUPPORTED = 16;
}
