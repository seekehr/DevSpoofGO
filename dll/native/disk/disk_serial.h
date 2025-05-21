#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

extern BOOL(WINAPI* g_real_DeviceIoControl)(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped
);

extern "C" {
    __declspec(dllexport) void SetSpoofedDiskSerial(const char* serial);
}

BOOL WINAPI HookedDeviceIoControlForDiskSerial(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped
);
