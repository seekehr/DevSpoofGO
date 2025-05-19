cmake_minimum_required(VERSION 3.10)
project(DevSpoofGO)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the DLL target
add_library(spoof_dll SHARED
    spoof_dll.cpp
    info/disk/vol_info.cpp
    info/os/os_info.cpp
    info/hardware/motherboard_serial.cpp
    info/network/wlan_info.cpp
    info/hardware/hardware_info.cpp
    info/hardware/bios_serial.cpp
    info/hardware/processor_id.cpp
    info/hardware/system_uuid.cpp
)

# Set include directories
target_include_directories(spoof_dll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/detours/include
)

# Link with Detours library
target_link_libraries(spoof_dll
    ${CMAKE_CURRENT_SOURCE_DIR}/detours/lib.X64/detours.lib
)