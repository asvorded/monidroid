cmake_minimum_required(VERSION 3.20)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(VCPKG_TARGET_TRIPLET x64-win-cpp20-release)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VCPKG_TARGET_TRIPLET x64-win-cpp20)
endif()

project(monidroid)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_compile_definitions(DEBUG)
endif()

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
endif()

add_subdirectory(common)

add_subdirectory(server)

add_subdirectory(iddcx-driver)

add_subdirectory(tests)
