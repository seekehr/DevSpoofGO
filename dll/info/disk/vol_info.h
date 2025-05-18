#pragma once
#include <windows.h>

// --- Function declarations ---

// Hook functions
BOOL WINAPI Hooked_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

BOOL WINAPI Hooked_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

// API for the injector
extern "C" {
    __declspec(dllexport) void SetSpoofedVolumeSerial(DWORD serial);
}

// Helper functions for hooking from spoof_dll.cpp
PVOID* GetRealGetVolumeInformationA();
PVOID* GetRealGetVolumeInformationW();
PVOID GetHookedGetVolumeInformationA();
PVOID GetHookedGetVolumeInformationW(); 