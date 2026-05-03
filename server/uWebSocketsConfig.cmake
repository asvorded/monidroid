cmake_minimum_required(VERSION 3.20)

include_guard()

include(FetchContent)

FetchContent_Declare(
    uSockets_content
    GIT_REPOSITORY https://github.com/uNetworking/uSockets
    GIT_SHALLOW ON
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(uSockets_content)
file(GLOB_RECURSE SOURCES ${usockets_content_SOURCE_DIR}/src/*.c)
add_library(uSockets ${SOURCES})
target_include_directories(uSockets PUBLIC ${usockets_content_SOURCE_DIR}/src)
target_compile_definitions(uSockets PRIVATE LIBUS_NO_SSL UWS_NO_ZLIB)

FetchContent_Declare(
    uWebSockets_content
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets
    GIT_TAG v20.37.0
    GIT_SHALLOW ON
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(uWebSockets_content)
add_library(uWebSockets INTERFACE)
target_include_directories(uWebSockets INTERFACE ${uwebsockets_content_SOURCE_DIR}/src/)
target_compile_definitions(uWebSockets INTERFACE LIBUS_NO_SSL UWS_NO_ZLIB)
target_link_libraries(uWebSockets INTERFACE uSockets)
