#include <windows.h>
#include "vol_info.h"

// --- Volume information related global variables ---
DWORD g_volumeSerial = 0xABCD1234;  // Spoofed volume serial

// --- Function pointers to original Windows API functions ---
// Pointer to the original GetVolumeInformationA function
static BOOL (WINAPI *Real_GetVolumeInformationA)(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPSTR, DWORD) = GetVolumeInformationA;

// Pointer to the original GetVolumeInformationW function
static BOOL (WINAPI *Real_GetVolumeInformationW)(LPCWSTR, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD) = GetVolumeInformationW;

// --- Hook functions ---

// Hook function for GetVolumeInformationA
BOOL WINAPI Hooked_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {

    // Call the original function first
    BOOL result = Real_GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
        lpFileSystemNameBuffer, nFileSystemNameSize);

    // If the original call succeeded and the caller provided a buffer for the serial number
    if (result && lpVolumeSerialNumber != nullptr) {
        // Overwrite the serial number with our spoofed value
        *lpVolumeSerialNumber = g_volumeSerial;
    }

    // Return the result of the original call
    return result;
}

// Hook function for GetVolumeInformationW
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

// Function to set the spoofed volume serial number
void SetSpoofedVolumeSerial(DWORD serial) {
    g_volumeSerial = serial;
}

// Function to get pointers to the original functions for hooking
PVOID* GetRealGetVolumeInformationA() {
    return reinterpret_cast<PVOID*>(&Real_GetVolumeInformationA);
}

PVOID* GetRealGetVolumeInformationW() {
    return reinterpret_cast<PVOID*>(&Real_GetVolumeInformationW);
}

// Function to get pointers to the hook functions
PVOID GetHookedGetVolumeInformationA() {
    return reinterpret_cast<PVOID>(Hooked_GetVolumeInformationA);
}

PVOID GetHookedGetVolumeInformationW() {
    return reinterpret_cast<PVOID>(Hooked_GetVolumeInformationW);
}
