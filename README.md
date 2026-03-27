# Monidroid project

Turn your mobile device into second monitor on any platform!

*Supported platforms (this list will be updated):*
- Windows 10 (>= 20H2)
- Linux (tested on the following distros)
    - Kubuntu 25.10 (KDE on Wayland)
    - CachyOS (GNOME on Wayland)

## Building

Necessary dependencies:
- `pkg-config` (**Windows:** `winget install bloodrock.pkg-config-lite`)
- `boost`: `Asio`, `Test`
- `libjpeg-turbo`
- `gstreamer` (**Windows:** set `GSTREAMER_ROOT_X86_64` environment variable after installation)
- `evdi`

To build Windows driver, use Visual Studio 2022
