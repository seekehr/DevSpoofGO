#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

// Typedefs for original WIDE function pointers, used by handlers if they need to call the true original API.
// These are passed from reg_info.cpp, which holds the actual Real_Reg...W pointers.
typedef LSTATUS (WINAPI *PFN_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
typedef LSTATUS (WINAPI *PFN_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME);
typedef LSTATUS (WINAPI *PFN_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS (WINAPI *PFN_RegCloseKey)(HKEY);
// No A versions needed here anymore

// Handler functions for certificate-related registry operations (WIDE versions only)
void Handle_RegOpenKeyExW_ForCertificates(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& status, bool& handled,
    PFN_RegOpenKeyExW original_RegOpenKeyExW_passthrough // Actual Real_RegOpenKeyExW from reg_info
);

void Handle_RegEnumKeyExW_ForCertificates(
    HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, 
    LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime,
    LSTATUS& status, bool& handled
    // This handler typically spoofs entirely or returns ERROR_NO_MORE_ITEMS,
    // so it doesn't usually call the original_RegEnumKeyExW itself.
);

void Handle_RegQueryValueExW_ForCertificates(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& status, bool& handled
    // This handler spoofs data for HKEY_SPOOFED_CERT.
);

void Handle_RegCloseKey_ForCertificates(
    HKEY hKey,
    LSTATUS& status, bool& handled
    // This handler manages g_openCertRootKeyHandles and HKEY_SPOOFED_CERT.
);

// Initializes certificate spoofing data.
// Now only takes PVOID for WIDE original functions as reg_info will handle A->W.
void InitializeCertificateSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW,
    PVOID pTrueOriginalRegEnumKeyExW,
    PVOID pTrueOriginalRegQueryValueExW,
    PVOID pTrueOriginalRegCloseKey
);

// Special HKEY value representing the spoofed certificate's registry key.
const HKEY HKEY_SPOOFED_CERT = (HKEY)(ULONG_PTR)0xDEADFACE;
