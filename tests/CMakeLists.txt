cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0167 NEW)

enable_testing()

include(CTest)

include(FindPkgConfig)

find_package(Boost REQUIRED COMPONENTS unit_test_framework process filesystem)

# EDID tests
add_executable(edid_tests edid.cpp)

target_link_libraries(edid_tests
    PRIVATE
        monidroid-common
        Boost::unit_test_framework
)

# USB tests
add_executable(usb_tests usb.cpp ../server/usbserver.cpp)

pkg_check_modules(libusb REQUIRED IMPORTED_TARGET
    libusb-1.0
)

target_link_libraries(usb_tests
    PRIVATE
        monidroid-common
        PkgConfig::libusb
        Boost::process
        Boost::filesystem
        Boost::unit_test_framework
)

add_test(NAME EDIDTests COMMAND edid_tests)
add_test(NAME USBServerTests COMMAND usb_tests)

if(WIN32)
    add_executable(win_input_tests wininput.cpp)

    target_compile_definitions(win_input_tests PRIVATE MD_SERVICE_LOG="${CMAKE_SOURCE_DIR}/servicelog.txt")

    target_link_libraries(win_input_tests
        PRIVATE
            monidroid-common
            monidroid-win32
            Boost::unit_test_framework
    )

    add_test(NAME WindowsInputTests COMMAND win_input_tests)
endif()
