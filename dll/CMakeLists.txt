cmake_minimum_required(VERSION 3.10)
project(spoof_dll C)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Change to x64 if needed
set(DETOURS_ARCH "X86")

include_directories(${CMAKE_SOURCE_DIR}/detours/include)

link_directories(${CMAKE_SOURCE_DIR}/detours/lib.${DETOURS_ARCH})

add_library(spoof_dll SHARED spoof_dll.c)

target_link_libraries(spoof_dll detours detoured)
