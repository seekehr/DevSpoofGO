#include <windows.h>
#include "vol_info.h"

DWORD g_volumeSerial = 0xABCD1234;

static BOOL (WINAPI *Real_GetVolumeInformationA)(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPSTR, DWORD) = GetVolumeInformationA;
static BOOL (WINAPI *Real_GetVolumeInformationW)(LPCWSTR, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD) = GetVolumeInformationW;

BOOL WINAPI Hooked_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
    BOOL result = Real_GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
        lpFileSystemNameBuffer, nFileSystemNameSize);
    if (result && lpVolumeSerialNumber != nullptr) {
        *lpVolumeSerialNumber = g_volumeSerial;
    }
    return result;
}

BOOL WINAPI Hooked_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
    BOOL result = Real_GetVolumeInformationW(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
        lpFileSystemNameBuffer, nFileSystemNameSize);
    if (result && lpVolumeSerialNumber != NULL) {
        *lpVolumeSerialNumber = g_volumeSerial;
    }
    return result;
}

void SetSpoofedVolumeSerial(DWORD serial) {
    g_volumeSerial = serial;
}

PVOID* GetRealGetVolumeInformationA() {
    return reinterpret_cast<PVOID*>(&Real_GetVolumeInformationA);
}

PVOID* GetRealGetVolumeInformationW() {
    return reinterpret_cast<PVOID*>(&Real_GetVolumeInformationW);
}

PVOID GetHookedGetVolumeInformationA() {
    return reinterpret_cast<PVOID>(Hooked_GetVolumeInformationA);
}

PVOID GetHookedGetVolumeInformationW() {
    return reinterpret_cast<PVOID>(Hooked_GetVolumeInformationW);
}
