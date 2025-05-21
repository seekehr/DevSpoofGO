#include <windows.h>
#include "vol_info.h"
#include <string>
#include <cstdio> // For swprintf_s
#include "../../detours/include/detours.h"

DWORD g_volumeSerial = 0xABCD1234;

BOOL (WINAPI *Real_GetVolumeInformationW)(LPCWSTR, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD) = GetVolumeInformationW;
BOOL (WINAPI *Real_GetVolumeInformationByHandleW)(HANDLE, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD) = GetVolumeInformationByHandleW;

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

BOOL WINAPI Hooked_GetVolumeInformationByHandleW(HANDLE hFile, LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
    BOOL result = Real_GetVolumeInformationByHandleW(hFile, lpVolumeNameBuffer, nVolumeNameSize,
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

PVOID* GetRealGetVolumeInformationW() {
    return reinterpret_cast<PVOID*>(&Real_GetVolumeInformationW);
}

PVOID GetHookedGetVolumeInformationW() {
    return reinterpret_cast<PVOID>(Hooked_GetVolumeInformationW);
}

// Helper to log messages (could be a shared utility)
static void LogVolInfo(const wchar_t* msg, LONG error_code = 0) {
    WCHAR buf[512];
    if (error_code != 0) {
        swprintf_s(buf, L"VOL_INFO_HOOK: %ls - Error: %ld (0x%08X)", msg, error_code, error_code);
    } else {
        swprintf_s(buf, L"VOL_INFO_HOOK: %ls", msg);
    }
    OutputDebugStringW(buf);
}

bool InitializeVolumeInfoHooks() {
    bool success = true;
    // Attach Volume Info hooks
    if (Real_GetVolumeInformationW) {
        LONG error_volinfo_w = DetourAttach(&(PVOID&)Real_GetVolumeInformationW, Hooked_GetVolumeInformationW);
        if (error_volinfo_w != NO_ERROR) {
            LogVolInfo(L"Failed to attach GetVolumeInformationW", error_volinfo_w);
            success = false;
        }
    } else {
        LogVolInfo(L"Real_GetVolumeInformationW is null, cannot attach hook.");
        success = false;
    }

    if (Real_GetVolumeInformationByHandleW) {
        LONG error_volinfo_h = DetourAttach(&(PVOID&)Real_GetVolumeInformationByHandleW, Hooked_GetVolumeInformationByHandleW);
        if (error_volinfo_h != NO_ERROR) {
            LogVolInfo(L"Failed to attach GetVolumeInformationByHandleW", error_volinfo_h);
            success = false;
        }
    } else {
        LogVolInfo(L"Real_GetVolumeInformationByHandleW is null, cannot attach hook.");
        success = false;
    }
    if (success) LogVolInfo(L"Volume Info hooks attached.");
    return success;
}

void CleanupVolumeInfoHooks() {
    // Detach Volume Info hooks
    if (Real_GetVolumeInformationByHandleW) {
        LONG error_volinfo_h = DetourDetach(&(PVOID&)Real_GetVolumeInformationByHandleW, Hooked_GetVolumeInformationByHandleW);
        if (error_volinfo_h != NO_ERROR) {
            LogVolInfo(L"Failed to detach GetVolumeInformationByHandleW", error_volinfo_h);
        }
    }
    if (Real_GetVolumeInformationW) {
        LONG error_volinfo_w = DetourDetach(&(PVOID&)Real_GetVolumeInformationW, Hooked_GetVolumeInformationW);
        if (error_volinfo_w != NO_ERROR) {
            LogVolInfo(L"Failed to detach GetVolumeInformationW", error_volinfo_w);
        }
    }
    LogVolInfo(L"Volume Info hooks detached.");
}
