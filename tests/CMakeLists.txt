add_executable(vhid_tests
    src/test_main.cpp
    src/test_reports.cpp
    src/test_descriptor.cpp
    src/test_in_process_transport.cpp
    src/test_capture.cpp
)

target_include_directories(vhid_tests PRIVATE include)
target_link_libraries(vhid_tests PRIVATE vhid_core vhid_mock vhid_capture)

add_test(NAME vhid_tests COMMAND vhid_tests)

# End-to-end CLI smoke test over the named-pipe mock transport.
if(WIN32 AND VHID_BUILD_CLI)
    add_test(
        NAME vhid_cli_smoke
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/scripts/smoke-mock.ps1"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()
