#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

// Typedefs for original WIDE function pointers
typedef LSTATUS (WINAPI *PFN_CV_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
typedef LSTATUS (WINAPI *PFN_CV_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD); // Not strictly needed if handlers don't call original directly
typedef LSTATUS (WINAPI *PFN_CV_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD); // Not strictly needed
typedef LSTATUS (WINAPI *PFN_CV_RegCloseKey)(HKEY); // Common, not strictly needed if handler doesn't call original

// Handler functions for CurrentVersion-related registry operations (W versions ONLY)
void Handle_RegOpenKeyExW_ForCurrentVersion(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& status, bool& handled,
    PFN_CV_RegOpenKeyExW original_RegOpenKeyExW_passthrough // Actual Real_RegOpenKeyExW from reg_info
);
void Handle_RegQueryValueExW_ForCurrentVersion(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& status, bool& handled
);
void Handle_RegGetValueW_ForCurrentVersion(
    HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags,
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& status, bool& handled
);
void Handle_RegCloseKey_ForCurrentVersion(
    HKEY hKey,
    LSTATUS& status, bool& handled
);

// Initializes CurrentVersion spoofing data.
// Now only takes PVOID for WIDE original functions.
void InitializeCurrentVersionSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW,
    PVOID pTrueOriginalRegQueryValueExW, // Kept for signature consistency, not stored/used by CV handlers
    PVOID pTrueOriginalRegGetValueW,     // Kept for signature consistency, not stored/used by CV handlers
    PVOID pTrueOriginalRegCloseKey       // Kept for signature consistency, not stored/used by CV handlers
);

// Exported function to optionally set a specific Product ID (example, not strictly required by prompt for now)
// extern "C" __declspec(dllexport) void SetSpoofedCVProductId(const char* productId);
