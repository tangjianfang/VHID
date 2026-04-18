add_executable(vhid_cli src/main.cpp)
target_link_libraries(vhid_cli PRIVATE vhid_sdk)
set_target_properties(vhid_cli PROPERTIES OUTPUT_NAME "vhid-cli")
