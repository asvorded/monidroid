cmake_minimum_required(VERSION 3.20)

include(${VCPKG_ROOT_DIR}/triplets/x64-windows-release.cmake)

set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_CXX_STANDARD=20" "-DCMAKE_CXX_STANDARD_REQUIRED=TRUE")