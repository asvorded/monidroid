cmake_minimum_required(VERSION 3.20)

add_library(monidroid-iddcx INTERFACE)

target_sources(monidroid-iddcx
    PUBLIC
        FILE_SET HEADERS
        FILES
            iddcx.h
)
