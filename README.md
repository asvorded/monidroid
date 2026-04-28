# Monidroid project

Turn your mobile device into second monitor on any platform!

*Supported platforms (this list will be updated):*
- Windows 10 (>= 20H2)
- Linux (tested on the following distros)
    - Kubuntu 25.10 (KDE on Wayland)
    - CachyOS (GNOME on Wayland)

## Building

Common dependencies:
- `pkg-config` (**Windows:** `winget install bloodrock.pkg-config-lite`)
- `boost`: `Asio`, `Test`, `Process`, `Program options`
- `libjpeg-turbo`
- `GStreamer`
- `libusb`

Linux specific dependencies:
- `evdi`
- `libevdev`

When building on Windows, please set up `CMAKE_PREFIX_PATH` for some dependencies (like `boost`)

To build Windows driver (**iddcx-driver**), use Visual Studio 2022
