cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0167 NEW)

#
# TARGET (default): server
#
set(SERVER_TARGET monidroid-server)
set(SERVER_VERSION 0.1.0)

add_executable(${SERVER_TARGET})
target_sources(${SERVER_TARGET} PRIVATE
    main.cpp
    echoserver.cpp
    server.cpp
    usbserver.cpp
    client.cpp
)

target_compile_definitions(${SERVER_TARGET} PRIVATE
    MD_SERVER_EXE_NAME="${SERVER_TARGET}"
    MD_SERVER_VERSION="${SERVER_VERSION}"
)

# LibJPEG-Turbo
if(LINUX)
    set(libjpeg-turbo_DIR /opt/libjpeg-turbo/lib64/cmake/libjpeg-turbo CACHE STRING "")
endif()

find_package(libjpeg-turbo REQUIRED)

# GStreamer
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH $ENV{GSTREAMER_ROOT_X86_64})
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GStreamer REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-rtsp-server-1.0
)

# libusb
pkg_check_modules(libusb REQUIRED IMPORTED_TARGET
    libusb-1.0
)

# Boost
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:\\Program Files\\boost_1_90_0\\lib64-msvc-14.3\\cmake")
endif()
find_package(Boost REQUIRED process program_options)

target_include_directories(${SERVER_TARGET} PRIVATE ${Boost_INCLUDE_DIRS})

# uWebSockets
find_package(uWebSockets PATHS ${CMAKE_CURRENT_SOURCE_DIR})

# JSON
find_package(nlohmann_json REQUIRED)

target_link_libraries(${SERVER_TARGET}
    PRIVATE
        monidroid-common
        libjpeg-turbo::turbojpeg
        PkgConfig::GStreamer
        PkgConfig::libusb
        Boost::process
        Boost::program_options
        uWebSockets
        nlohmann_json::nlohmann_json
)

if(LINUX)
    #
    # TARGET (default): Linux library
    #
    add_library(monidroid-linux)

    target_sources(monidroid-linux
        PRIVATE
            linux/evdi.cpp
            linux/misc.cpp

        PUBLIC
            FILE_SET HEADERS
            FILES
                native.h
    )

    # EVDI library
    find_library(evdi libevdi.so)

    # libevdev
    pkg_check_modules(libevdev REQUIRED IMPORTED_TARGET
        libevdev
    )

    target_link_libraries(monidroid-linux
        PRIVATE
            evdi
            PkgConfig::libevdev

        PUBLIC
            monidroid-common
    )

    target_link_libraries(${SERVER_TARGET}
        PRIVATE
            monidroid-linux
    )
elseif(WIN32)
    #
    # TARGET (default): Windows library
    #
    add_library(monidroid-win32)

    target_sources(monidroid-win32
        PRIVATE
            win32/iddcx.cpp
            win32/misc.cpp

        PUBLIC
            FILE_SET HEADERS
            FILES
                native.h
    )

    target_link_libraries(monidroid-win32
        PRIVATE
            monidroid-iddcx
            DXGI.lib
            OneCoreUAP.lib
            D3D11.lib

        PUBLIC
            monidroid-common
    )

    target_link_libraries(${SERVER_TARGET}
        PRIVATE
            monidroid-win32
    )
endif()
