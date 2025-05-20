#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

// Typedefs for original function pointers, used by handlers if they need to call the true original API.
typedef LSTATUS (WINAPI *PFN_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
typedef LSTATUS (WINAPI *PFN_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME);
typedef LSTATUS (WINAPI *PFN_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS (WINAPI *PFN_RegCloseKey)(HKEY);

// Handler functions for certificate-related registry operations
void Handle_RegOpenKeyExW_ForCertificates(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& status, bool& handled,
    PFN_RegOpenKeyExW original_RegOpenKeyExW 
);

void Handle_RegEnumKeyExW_ForCertificates(
    HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, 
    LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime,
    LSTATUS& status, bool& handled
);

void Handle_RegQueryValueExW_ForCertificates(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& status, bool& handled
);

void Handle_RegCloseKey_ForCertificates(
    HKEY hKey,
    LSTATUS& status, bool& handled
);

// Initializes certificate spoofing data and stores true original function pointers.
void InitializeCertificateSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW,
    PVOID pTrueOriginalRegEnumKeyExW,
    PVOID pTrueOriginalRegQueryValueExW,
    PVOID pTrueOriginalRegCloseKey
);

// Special HKEY value representing the spoofed certificate's registry key.
const HKEY HKEY_SPOOFED_CERT = (HKEY)(ULONG_PTR)0xDEADFACE;
