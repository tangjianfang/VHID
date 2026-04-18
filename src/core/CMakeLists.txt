add_library(vhid_core STATIC
    src/report_descriptor.cpp
    src/reports.cpp
)

target_include_directories(vhid_core PUBLIC include)
target_compile_features(vhid_core PUBLIC cxx_std_17)
