add_library(vhid_capture STATIC
    src/device_profile.cpp
    src/device_enumerator.cpp
    src/device_inspector.cpp
    src/descriptor_builder.cpp
)

target_include_directories(vhid_capture PUBLIC include)
target_link_libraries(vhid_capture PUBLIC vhid_core)
target_link_libraries(vhid_capture PRIVATE nlohmann_json)

if(WIN32)
    target_link_libraries(vhid_capture PRIVATE setupapi hid)
endif()
