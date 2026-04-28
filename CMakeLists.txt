cmake_minimum_required(VERSION 3.20)

project(monidroid)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_compile_definitions(DEBUG)
endif()

add_subdirectory(common)

add_subdirectory(server)

add_subdirectory(iddcx-driver)

add_subdirectory(tests)
