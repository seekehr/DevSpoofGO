#pragma once
#include <windows.h>

BOOL WINAPI Hooked_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

BOOL WINAPI Hooked_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

extern "C" {
    __declspec(dllexport) void SetSpoofedVolumeSerial(DWORD serial);
}

PVOID* GetRealGetVolumeInformationA();
PVOID* GetRealGetVolumeInformationW();
PVOID GetHookedGetVolumeInformationA();
PVOID GetHookedGetVolumeInformationW();