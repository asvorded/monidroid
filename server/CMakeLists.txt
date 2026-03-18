cmake_minimum_required(VERSION 3.20)

project(monidroid-linux)

# TARGET: default
add_executable(default)
target_sources(
    default PRIVATE
        src/main.cpp
        src/echoserver.cpp
        src/server.cpp
        src/client.cpp
        src/utils.cpp
)

target_compile_features(default PRIVATE cxx_std_20)

# LibJPEG-Turbo
if(LINUX)
    set(libjpeg-turbo_DIR CACHE STRING /opt/libjpeg-turbo/lib64/cmake/libjpeg-turbo)    
endif()

find_package(libjpeg-turbo REQUIRED)

# GStreamer
if(WIN32)
    list(APPEND CMAKE_PREFIX_PATH $ENV{GSTREAMER_ROOT_X86_64})
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(
    GStreamer REQUIRED IMPORTED_TARGET
        gstreamer-1.0
        gstreamer-audio-1.0
        gstreamer-pbutils-1.0
)

# Boost
if (WIN32)
    list(APPEND CMAKE_PREFIX_PATH "C:\\Program Files\\boost_1_90_0")
endif()
find_package(Boost REQUIRED)

target_include_directories(default PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(default PRIVATE
    monidroid-common
    libjpeg-turbo::turbojpeg
    PkgConfig::GStreamer
)

# EVDI library
if(LINUX)
    find_library(evdi libevdi.so)
    
    target_link_libraries(default PRIVATE
        evdi
    )
endif()

# TARGET: test
add_executable(test playground/ooo.cpp)

target_compile_features(test PRIVATE cxx_std_20)

target_link_libraries(test PRIVATE evdi PkgConfig::GStreamer)

