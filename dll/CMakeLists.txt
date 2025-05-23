cmake_minimum_required(VERSION 3.10)
project(DevSpoofGO)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the DLL target
add_library(spoof_dll SHARED
    spoof_dll.cpp
    native/disk/vol_info.cpp
    native/os/os_info.cpp
    native/hardware/motherboard_serial.cpp
    native/network/wlan_info.cpp
    native/hardware/hardware_info.cpp
    native/hardware/bios_serial.cpp
    native/hardware/processor_id.cpp
    native/hardware/system_uuid.cpp
    native/disk/disk_serial.cpp
    reg/certificates_info.cpp
    reg/machine_guid.cpp
    reg/reg_info.cpp
    reg/current_version.cpp
    wmi/wmi_handler.cpp
    wmi/wmi_utils.cpp
    wmi/wmi_bios_serial.cpp
    wmi/wmi_processor.cpp
    wmi/wmi_physical_memory.cpp
)


# Set include directories
target_include_directories(spoof_dll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/detours/include
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/um"
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.22621.0/ucrt"
)

# Define UNICODE and _UNICODE for wide character support
target_compile_definitions(spoof_dll PRIVATE UNICODE _UNICODE)

# Link with Detours library
target_link_libraries(spoof_dll
    ${CMAKE_CURRENT_SOURCE_DIR}/detours/lib.X64/detours.lib
    wbemuuid.lib
)