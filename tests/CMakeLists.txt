cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0167 NEW)

enable_testing()

include(CTest)

include(FindPkgConfig)

find_package(Boost REQUIRED COMPONENTS unit_test_framework Process)

# EDID tests
add_executable(edid_tests edid.cpp)

target_link_libraries(edid_tests
    PRIVATE
        monidroid-common
        Boost::unit_test_framework
)

# USB tests
add_executable(usb_tests usb.cpp)

pkg_check_modules(libusb REQUIRED IMPORTED_TARGET
    libusb-1.0
)

target_include_directories(usb_tests PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(usb_tests
    PRIVATE
        monidroid-common
        PkgConfig::libusb
        Boost::process
        Boost::unit_test_framework
)

add_test(NAME EDIDTests COMMAND edid_tests)
