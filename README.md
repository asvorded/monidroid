# Monidroid project

Turn your mobile device into second monitor on any platform!

### ✨ Features:
- Duplicating and extending desktop with mobile devices
- Connection via Wi-Fi and USB
- Touch input
- Run on system startup support

### ✅ Tested platforms (this list will be updated):
- Windows 10 22H2
- Linux:
    - Kubuntu 25.10 (KDE on Wayland)
    - CachyOS (GNOME and KDE on Wayland)
    - Fedora 44 (GNOME on Wayland)

## 🔨 Building from source

### Installing dependencies on Linux

All needed dependencies can be found in `server/CMakeLists.txt`. Here are the commands for some distros to install them:

Ubuntu:
``` bash
sudo apt update && sudo apt -y upgrade
sudo apt install cmake build-essential pkg-config libboost-dev nlohmann-json3-dev libusb-1.0-0-dev libevdev-dev

# Ubuntu version of libjpeg-turbo is too old
curl -LO https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.3/libjpeg-turbo-official_3.1.3_amd64.deb && sudo apt install ./libjpeg-turbo-official_3.1.3_amd64.deb
```

Arch-based:
``` bash
sudo pacman -Syu base-devel cmake pkgconf boost nlohmann-json libusb libevdev libjpeg-turbo
```

Fedora:
TODO

### Building evdi

The project uses a fork of [EVDI](https://github.com/DisplayLink/evdi), so to build the server you have two options:
1. Use `evdi-dkms` package from your distro (keep in mind that during deploy the project-provided module will be used)
2. Build the project-provided version (from project root):
``` bash
cd evdi/module
make
sudo make install
cd ../library
make
sudo make install
```

### Building contol panel

Requires `node` and `npm`. npm commands in `package.json:`
- `ui-dev` - start UI dev server
- `app-dev` - start panel in debug mode (`ui-dev` must be run first)
- `app-build` - build panel release

### Obtaining adb

`adb` is needed for the USB server and deployment.
Use official Android SDK Platform Tools or download `adb` manually and add the adb folder to `PATH`.

### Installing dependencies on Windows

The project is set up for using **vcpkg**. If you are using Visual Studio:
1. Make sure that _vcpkg package manager_ component is installed in Visual Studio Installer
1. In Visual Studio, open terminal and run `vcpkg integrate install`
1. _Project -> Configure Cache_

If not, configure CMake with vcpkg toolchain (`-DCMAKE_TOOLCHAIN_FILE=$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`)
([the official documentation](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started)).

Alternatively, you can manually install every needed package and set `CMAKE_PREFIX_PATH`.

### Building and deploying Windows driver

To build Windows driver (`iddcx-driver`), use Visual Studio 2022:
1. Install Windows Driver Kit
1. Install _Windows Driver Kit_ component in Visual Studio Installer

To deploy Windows driver, you need a virtual machine configured following [this tutorial](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/provision-a-target-computer).
Then, configure deployment in `MonidroidDriver` project properties:

1. Go to _Configuration Properties -> Driver Install -> Deployment_
1. Select configured computer
1. Tick "_Remove previous driver versions before deployment_"
1. _Driver Installation Options -> Hardware ID Driver Update_ -> Set `Root\MonidroidDriver`

## 🛟 Troubleshooting

### Error during installation process (com.monidroid.driver): Execution failed (Unexpected exit code: 3): "dkms install /opt/monidroid/driver"

Retry installation. If the error persists, please open an issue.

### Error during installation process (com.monidroid.driver): Execution failed (Unexpected exit code: 21): "dkms install /opt/monidroid/driver"

This error occurs during driver installation if kernel headers and build tools are not installed since installer-provided graphics driver must be built on the target system.

First, update your system.

Ubuntu:
``` bash
sudo apt update && sudo apt -y upgrade
```

Fedora:
``` bash
sudo dnf upgrade --refresh
```

Arch-based:
``` bash
sudo pacman -Syu
```

If the error persists, install kernel headers and build tools by the following commands:

Ubuntu:
``` bash
sudo apt install build-essential linux-headers-$(uname -r)
```

Fedora:
``` bash
sudo dnf install kernel-devel kernel-headers
```

Arch-based:
``` bash
sudo pacman base-devel linux-headers # Choose proper package (e.g. linux-lts-headers) according to your kernel
```

Then reboot your PC and retry installation.

### Computer is not visible in the "Detected devices" section

Check firewall settings.

TODO: Add ufw rules on Linux

### Monitor is not visible in GNOME settings

Run `sudo bash -c "echo 1 > /sys/devices/evdi/add"` in terminal before connection

This is a GNOME bug. We will try to address it by applying another /dev/dri/cardX managenent approach.

### Cannot connect on Windows, "failed to connect to ... from ... after 5000ms"

It is a standard Windows network layer behavior under debugger :). Disable debug by `bcdedit /debug off`, restart the system and try again.

### Cannot connect through USB on Windows

Please check server logs. If they contain at least one of these messages:
- `[<timestamp>, USB] Setup API: Cannot open Android device`
- `error: device <serial number> not found`

then perform the following steps:
1. Disconnect mobile device
1. Run `adb kill-server` from terminal
1. Connect mobile device again

If this does not help, please open an issue.

For more info, open this [source code line](https://github.com/asvorded/monidroid/blob/main/server/usbserver.cpp#L199).