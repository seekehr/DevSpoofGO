#include <windows.h>
#include <cstring>    
#include "os_info.h"
#include <string>
#include <lmcons.h> 
#include <cstdio>   
#include "../../detours/include/detours.h"

static std::wstring g_spoofedComputerName = L"SPOOFED_PC";

// Initialize function pointers to the real Windows API functions
BOOL (WINAPI *Real_GetComputerNameW)(LPWSTR lpBuffer, LPDWORD nSize) = GetComputerNameW;
BOOL (WINAPI *Real_GetComputerNameExW)(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize) = GetComputerNameExW;


BOOL WINAPI Hooked_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize) {
    OutputDebugStringW(L"Hooked_GetComputerNameW called");
    if (nSize == NULL || lpBuffer == NULL) {
        return FALSE; // Invalid arguments
    }

    size_t spoofedNameLen = g_spoofedComputerName.length();
    if (*nSize > spoofedNameLen) {
        wcscpy_s(lpBuffer, *nSize, g_spoofedComputerName.c_str());
        *nSize = (DWORD)spoofedNameLen;
        return TRUE;
    } else {
        *nSize = (DWORD)(spoofedNameLen + 1); // +1 for null terminator
        SetLastError(ERROR_BUFFER_OVERFLOW);
        return FALSE;
    }
}

BOOL WINAPI Hooked_GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize) {
    switch (NameType) {
        case ComputerNameNetBIOS:
        case ComputerNameDnsHostname:
        case ComputerNameDnsDomain: // May also want to spoof domain if desired
        case ComputerNameDnsFullyQualified:
        case ComputerNamePhysicalNetBIOS:
        case ComputerNamePhysicalDnsHostname:
        case ComputerNamePhysicalDnsDomain:
        case ComputerNamePhysicalDnsFullyQualified: {
            if (nSize == NULL || lpBuffer == NULL) {
                return FALSE; // Invalid arguments
            }
            size_t spoofedNameLen = g_spoofedComputerName.length();
            if (NameType == ComputerNameDnsFullyQualified || NameType == ComputerNamePhysicalDnsFullyQualified) {
                std::wstring qualifiedName = g_spoofedComputerName + L".spoofdomain.local";
                spoofedNameLen = qualifiedName.length();
                if (*nSize > spoofedNameLen) {
                    wcscpy_s(lpBuffer, *nSize, qualifiedName.c_str());
                    *nSize = (DWORD)spoofedNameLen;
                    return TRUE;
                } else {
                    *nSize = (DWORD)(spoofedNameLen + 1);
                    SetLastError(ERROR_BUFFER_OVERFLOW);
                    return FALSE;
                }
            } else {
                 if (*nSize > spoofedNameLen) {
                    wcscpy_s(lpBuffer, *nSize, g_spoofedComputerName.c_str());
                    *nSize = (DWORD)spoofedNameLen;
                    return TRUE;
                } else {
                    *nSize = (DWORD)(spoofedNameLen + 1);
                    SetLastError(ERROR_BUFFER_OVERFLOW);
                    return FALSE;
                }
            }
        }
        default:
            // Call the original function for other name types we don't explicitly spoof
            return Real_GetComputerNameExW(NameType, lpBuffer, nSize);
    }
}

// --- Exported C function to set the spoofed name ---
extern "C" {
    __declspec(dllexport) void SetSpoofedComputerName(const wchar_t* name) {
        if (name) {
            g_spoofedComputerName = name;
        } 
    }
}

// --- Helper for logging ---
static void LogOSInfo(const wchar_t* msg, LONG error_code = 0) {
    WCHAR buf[512];
    if (error_code != 0) {
        swprintf_s(buf, L"OS_INFO_HOOK: %ls - Error: %ld (0x%08X)", msg, error_code, error_code);
    } else {
        swprintf_s(buf, L"OS_INFO_HOOK: %ls", msg);
    }
    OutputDebugStringW(buf);
}

// --- Initialization and Cleanup Functions ---
bool InitializeOSInfoHooks() {
    bool success = true;
    if (Real_GetComputerNameW) {
        LONG error = DetourAttach(&(PVOID&)Real_GetComputerNameW, Hooked_GetComputerNameW);
        if (error != NO_ERROR) {
            LogOSInfo(L"Failed to attach GetComputerNameW", error);
            success = false;
        }
    } else {
        LogOSInfo(L"Real_GetComputerNameW is null. Cannot attach hook.");
        success = false;
    }

    if (Real_GetComputerNameExW) {
        LONG error = DetourAttach(&(PVOID&)Real_GetComputerNameExW, Hooked_GetComputerNameExW);
        if (error != NO_ERROR) {
            LogOSInfo(L"Failed to attach GetComputerNameExW", error);
            success = false;
        }
    } else {
        LogOSInfo(L"Real_GetComputerNameExW is null. Cannot attach hook.");
        success = false; 
    }

    return success;
}

void CleanupOSInfoHooks() {
    if (Real_GetComputerNameExW) { // Detach in reverse order of attach if dependencies exist, though not strictly necessary here
        LONG error = DetourDetach(&(PVOID&)Real_GetComputerNameExW, Hooked_GetComputerNameExW);
        if (error != NO_ERROR) LogOSInfo(L"Failed to detach GetComputerNameExW", error);
    }
    if (Real_GetComputerNameW) {
        LONG error = DetourDetach(&(PVOID&)Real_GetComputerNameW, Hooked_GetComputerNameW);
        if (error != NO_ERROR) LogOSInfo(L"Failed to detach GetComputerNameW", error);
    }
}
