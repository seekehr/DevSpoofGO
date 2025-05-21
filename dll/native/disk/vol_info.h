#pragma once
#include <windows.h>

// Function pointers to original Windows API functions
extern BOOL (WINAPI *Real_GetVolumeInformationW)(
    LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
);

extern BOOL (WINAPI *Real_GetVolumeInformationByHandleW)(
    HANDLE hFile,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
);

// Hooked function prototypes
BOOL WINAPI Hooked_GetVolumeInformationW(
    LPCWSTR lpRootPathName,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
);

BOOL WINAPI Hooked_GetVolumeInformationByHandleW(
    HANDLE hFile,
    LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize,
    LPDWORD lpVolumeSerialNumber,
    LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags,
    LPWSTR lpFileSystemNameBuffer,
    DWORD nFileSystemNameSize
);

// Exported functions to set spoofed values (if needed, examples)
extern "C" {
    __declspec(dllexport) void SetSpoofedVolumeSerial(DWORD serialNumber);
}

PVOID* GetRealGetVolumeInformationW();
PVOID GetHookedGetVolumeInformationW();

// Initialization and cleanup functions
bool InitializeVolumeInfoHooks();
void CleanupVolumeInfoHooks();