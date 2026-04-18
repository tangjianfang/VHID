if(NOT WIN32)
    return()
endif()

add_executable(consumer_app src/main.cpp)
target_link_libraries(consumer_app PRIVATE vhid_core setupapi hid)
