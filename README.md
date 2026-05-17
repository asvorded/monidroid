# Monidroid project

Turn your mobile device into second monitor on any platform!

Features:
- Duplicating and extending desktop with mobile devices
- Connection via Wi-Fi and USB
- Touch input
- Run on system startup support

*Tested platforms (this list will be updated):*
- Windows 10 22H2
- Linux:
    - Kubuntu 25.10 (KDE on Wayland)
    - CachyOS (GNOME and KDE on Wayland)
    - Fedora 44 (GNOME on Wayland)

## Building on Linux

### Installing server dependencies

All needed dependencies can be found in `server/CMakeLists.txt`. Here are the commands for some distros to install them:

Ubuntu:
``` bash
sudo apt update && sudo apt -y upgrade
sudo apt install cmake build-essential pkg-config libgstreamer1.0-dev libgstrtspserver-1.0-dev libboost-dev nlohmann-json3-dev libusb-1.0-0-dev libevdev-dev

# Ubuntu version of libjpeg-turbo is too old
curl -O https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.3/libjpeg-turbo-official_3.1.3_amd64.deb && apt install libjpeg-turbo-official_3.1.3_amd64.deb
```

Arch-based:
TODO

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
- `app-build` - build release of panel

### Deploying (making an installer)

``` bash
chmod a+x ./deploy/deploy-linux.sh
./deploy/deploy-linux.sh
```

## Building on Windows

### Installing pkg-config

``` cmd
winget install --id bloodrock.pkg-config-lite
```

### Installing dependencies

You have two options:
1. Manually download every package and set up `CMAKE_PREFIX_PATH`
2. Use `vcpkg`

### Building and deploying driver

To build Windows driver (`iddcx-driver`), use Visual Studio 2022

To deploy Windows driver, you need a virtual machine configured following [this tutorial](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/provision-a-target-computer).
Then, configure deployment in `MonidroidDriver` project properties:

1. Go to _Driver -> Driver Install_
1. Select configured computer
1. Tick "Remove before installation"
1. Set `Root\MonidroidDriver`

## Troubleshooting

### Installer fails on dkms install with exit code 3

Retry installation. If the error keeps appearing, please open an issue.

### Monitor is not visible in GNOME settings

Run `sudo bash -c "echo 1 > /sys/devices/evdi/add"` in terminal before connection

This is a GNOME bug. We will try to address it by applying another /dev/dri/cardX managenent approach

### Cannot connect on Windows, "failed to connect to ... from ... after 5000ms"

It is a standard Windows network layer behavior under debugger :). Disable debug by `bcdedit /debug off`, restart and try again.