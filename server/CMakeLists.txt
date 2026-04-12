cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0167 NEW)

#
# TARGET (default): server
#
set(SERVER_TARGET server)

add_executable(${SERVER_TARGET})
target_sources(${SERVER_TARGET} PRIVATE
    main.cpp
    echoserver.cpp
    server.cpp
    client.cpp
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
    gstreamer-audio-1.0
    gstreamer-pbutils-1.0
    gstreamer-rtsp-server-1.0
)

# Boost
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:\\Program Files\\boost_1_90_0")
endif()
find_package(Boost REQUIRED)

target_include_directories(${SERVER_TARGET} PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(${SERVER_TARGET}
    PRIVATE
        monidroid-common
        libjpeg-turbo::turbojpeg
        PkgConfig::GStreamer
)

if(WIN32)

elseif(LINUX)
endif()

if(LINUX)
    #
    # TARGET (default): Linux library
    #
    add_library(monidroid-linux)

    target_sources(monidroid-linux
        PRIVATE
            linux/evdi.cpp
        
        PUBLIC
            FILE_SET HEADERS
            FILES
                native.h
    )

    # EVDI library
    find_library(evdi libevdi.so)

    target_link_libraries(monidroid-linux
        PUBLIC
            evdi
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

        PUBLIC
            FILE_SET HEADERS
            FILES
                native.h
    )

    target_link_libraries(monidroid-win32
        PRIVATE
            monidroid-iddcx

        PUBLIC
            monidroid-common
    )

    target_link_libraries(${SERVER_TARGET}
        PRIVATE
            monidroid-win32
    )
endif()