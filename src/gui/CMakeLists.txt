add_executable(vhid_gui WIN32
    src/main.cpp
)

set_target_properties(vhid_gui PROPERTIES OUTPUT_NAME "vhid-gui")

target_compile_definitions(vhid_gui PRIVATE UNICODE _UNICODE)

target_link_libraries(vhid_gui PRIVATE
    vhid_core
    vhid_mock
    vhid_sdk
    vhid_capture
)

if(WIN32)
    target_link_libraries(vhid_gui PRIVATE comctl32 comdlg32 uxtheme gdi32)
endif()
