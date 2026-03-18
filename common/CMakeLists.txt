cmake_minimum_required(VERSION 3.20)

project(monidroid-common)

add_library(monidroid-common INTERFACE)

target_sources(monidroid-common INTERFACE
    FILE_SET HEADERS
    FILES
        monidroid.h
        monidroid/logger.h
)