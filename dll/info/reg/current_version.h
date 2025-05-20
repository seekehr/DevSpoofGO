#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

// Typedefs for original function pointers (W versions)
typedef LSTATUS (WINAPI *PFN_CV_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
typedef LSTATUS (WINAPI *PFN_CV_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS (WINAPI *PFN_CV_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
typedef LSTATUS (WINAPI *PFN_CV_RegCloseKey)(HKEY); // Common

// Typedefs for original function pointers (A versions)
typedef LSTATUS (WINAPI *PFN_CV_RegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
typedef LSTATUS (WINAPI *PFN_CV_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS (WINAPI *PFN_CV_RegGetValueA)(HKEY, LPCSTR, LPCSTR, DWORD, LPDWORD, PVOID, LPDWORD);

// Handler functions for CurrentVersion-related registry operations (W versions)
void Handle_RegOpenKeyExW_ForCurrentVersion(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& status, bool& handled,
    PFN_CV_RegOpenKeyExW original_RegOpenKeyExW_param
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
); // Common

// Handler functions for CurrentVersion-related registry operations (A versions)
void Handle_RegOpenKeyExA_ForCurrentVersion(
    HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& status, bool& handled,
    PFN_CV_RegOpenKeyExA original_RegOpenKeyExA_param
);
void Handle_RegQueryValueExA_ForCurrentVersion(
    HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& status, bool& handled
);
void Handle_RegGetValueA_ForCurrentVersion(
    HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags,
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& status, bool& handled
);

// Initializes CurrentVersion spoofing data and stores true original function pointers.
void InitializeCurrentVersionSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW,
    PVOID pTrueOriginalRegQueryValueExW,
    PVOID pTrueOriginalRegGetValueW,
    PVOID pTrueOriginalRegCloseKey, // Common
    PVOID pTrueOriginalRegOpenKeyExA,
    PVOID pTrueOriginalRegQueryValueExA,
    PVOID pTrueOriginalRegGetValueA
);

// Exported function to optionally set a specific Product ID (example, not strictly required by prompt for now)
// extern "C" __declspec(dllexport) void SetSpoofedCVProductId(const char* productId);
