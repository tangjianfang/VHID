add_library(vhid_mock STATIC
    src/in_process_transport.cpp
    src/named_pipe_transport.cpp
)

target_include_directories(vhid_mock PUBLIC include)
target_link_libraries(vhid_mock PUBLIC vhid_core)

if(WIN32)
    target_link_libraries(vhid_mock PUBLIC)
endif()
