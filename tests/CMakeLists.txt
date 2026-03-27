cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0167 NEW)

enable_testing()

include(CTest)

add_executable(edid-tests edid.cpp)

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

target_link_libraries(edid-tests
    PRIVATE
        monidroid-common
        Boost::unit_test_framework
)

add_test(NAME EDIDTests COMMAND edid-tests)
