add_library(vhid_sdk STATIC
    src/vhid_device.cpp
    src/driver_transport.cpp
)

target_include_directories(vhid_sdk PUBLIC include)
target_link_libraries(vhid_sdk PUBLIC vhid_core vhid_mock)

if(WIN32)
    target_link_libraries(vhid_sdk PUBLIC setupapi hid)
endif()
