cmake_minimum_required(VERSION 3.20)
project(VHID CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

option(VHID_BUILD_TESTS   "Build unit tests"     ON)
option(VHID_BUILD_CLI     "Build CLI tool"       ON)
option(VHID_BUILD_GUI     "Build Win32 GUI"      ON)
option(VHID_BUILD_SAMPLES "Build sample apps"    ON)

enable_testing()

# Vendored third-party single-header libraries
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_subdirectory(src/core)
add_subdirectory(src/mock)
add_subdirectory(src/sdk)
add_subdirectory(src/capture)

if(VHID_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

if(VHID_BUILD_GUI AND WIN32)
    add_subdirectory(src/gui)
endif()

if(VHID_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(VHID_BUILD_SAMPLES)
    add_subdirectory(samples/consumer_app)
endif()
