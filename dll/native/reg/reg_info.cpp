#include "reg_info.h"
#include "certificates_info.h" // Corrected path
#include "machine_guid.h"      // Corrected path
#include "current_version.h"   // Added for CurrentVersion spoofing
#include <cstdio>              
#include "../../detours/include/detours.h" // Added for DetourDetach

static LSTATUS (WINAPI *Real_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY) = nullptr;
static LSTATUS (WINAPI *Real_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME) = nullptr;
static LSTATUS (WINAPI *Real_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
static LSTATUS (WINAPI *Real_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;
static LSTATUS (WINAPI *Real_RegCloseKey)(HKEY) = nullptr;

static LSTATUS (WINAPI *Real_RegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY) = nullptr;
static LSTATUS (WINAPI *Real_RegEnumKeyExA)(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, PFILETIME) = nullptr;
static LSTATUS (WINAPI *Real_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
static LSTATUS (WINAPI *Real_RegGetValueA)(HKEY, LPCSTR, LPCSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;

bool InitializeRegistryHooksAndHandlers() {
    HMODULE hAdvapi32 = GetModuleHandleW(L"advapi32.dll");
    if (!hAdvapi32) {
        hAdvapi32 = LoadLibraryW(L"advapi32.dll");
        if (!hAdvapi32) {
            OutputDebugStringW(L"REG_INFO: CRITICAL - Failed to load advapi32.dll via LoadLibraryW.");
            return false;
        }
    }

    Real_RegOpenKeyExW = (decltype(Real_RegOpenKeyExW))GetProcAddress(hAdvapi32, "RegOpenKeyExW");

    Real_RegEnumKeyExW = (decltype(Real_RegEnumKeyExW))GetProcAddress(hAdvapi32, "RegEnumKeyExW");

    Real_RegQueryValueExW = (decltype(Real_RegQueryValueExW))GetProcAddress(hAdvapi32, "RegQueryValueExW");

    Real_RegGetValueW = (decltype(Real_RegGetValueW))GetProcAddress(hAdvapi32, "RegGetValueW");

    Real_RegCloseKey = (decltype(Real_RegCloseKey))GetProcAddress(hAdvapi32, "RegCloseKey");

    Real_RegOpenKeyExA = (decltype(Real_RegOpenKeyExA))GetProcAddress(hAdvapi32, "RegOpenKeyExA");

    Real_RegEnumKeyExA = (decltype(Real_RegEnumKeyExA))GetProcAddress(hAdvapi32, "RegEnumKeyExA");

    Real_RegQueryValueExA = (decltype(Real_RegQueryValueExA))GetProcAddress(hAdvapi32, "RegQueryValueExA");

    Real_RegGetValueA = (decltype(Real_RegGetValueA))GetProcAddress(hAdvapi32, "RegGetValueA");

    bool success = Real_RegOpenKeyExW && Real_RegEnumKeyExW && Real_RegQueryValueExW && 
                   Real_RegGetValueW && Real_RegCloseKey &&
                   Real_RegOpenKeyExA && Real_RegEnumKeyExA && Real_RegQueryValueExA &&
                   Real_RegGetValueA;

    if (!success) {
        OutputDebugStringW(L"REG_INFO: CRITICAL - Failed to get addresses for one or more registry functions (A/W). Cannot proceed with Detours.");
        return false; // Cannot proceed if function pointers are null
    }

    InitializeCertificateSpoofing_Centralized(
        reinterpret_cast<PVOID>(Real_RegOpenKeyExW),
        reinterpret_cast<PVOID>(Real_RegEnumKeyExW),
        reinterpret_cast<PVOID>(Real_RegQueryValueExW),
        reinterpret_cast<PVOID>(Real_RegCloseKey),
        reinterpret_cast<PVOID>(Real_RegOpenKeyExA),
        reinterpret_cast<PVOID>(Real_RegEnumKeyExA),
        reinterpret_cast<PVOID>(Real_RegQueryValueExA)
    );
    InitializeMachineGuid_Centralized(
        reinterpret_cast<PVOID>(Real_RegQueryValueExW),
        reinterpret_cast<PVOID>(Real_RegGetValueW),
        reinterpret_cast<PVOID>(Real_RegQueryValueExA),
        reinterpret_cast<PVOID>(Real_RegGetValueA)
    );
    InitializeCurrentVersionSpoofing_Centralized(
        reinterpret_cast<PVOID>(Real_RegOpenKeyExW),
        reinterpret_cast<PVOID>(Real_RegQueryValueExW),
        reinterpret_cast<PVOID>(Real_RegGetValueW),
        reinterpret_cast<PVOID>(Real_RegCloseKey),
        reinterpret_cast<PVOID>(Real_RegOpenKeyExA),
        reinterpret_cast<PVOID>(Real_RegQueryValueExA),
        reinterpret_cast<PVOID>(Real_RegGetValueA)
    );

    LONG detourError;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegOpenKeyExW), Hooked_RegOpenKeyExW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegEnumKeyExW), Hooked_RegEnumKeyExW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegQueryValueExW), Hooked_RegQueryValueExW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegGetValueW), Hooked_RegGetValueW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegCloseKey), Hooked_RegCloseKey);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegOpenKeyExA), Hooked_RegOpenKeyExA);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegEnumKeyExA), Hooked_RegEnumKeyExA);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegQueryValueExA), Hooked_RegQueryValueExA);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&Real_RegGetValueA), Hooked_RegGetValueA);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourTransactionCommit();
    if (detourError != NO_ERROR) {
        success = false;
    }

    if (!success) {
        OutputDebugStringW(L"REG_INFO: CRITICAL - One or more DetourAttach calls failed OR transaction commit failed.");
        // Consider if we should attempt to DetachAll or cleanup if some attached but others failed.
        // For now, returning false indicates a critical failure in hooking.
    }

    return success;
}

void CleanupRegistryHooksAndHandlers() {
    LONG detourError;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (Real_RegOpenKeyExW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegOpenKeyExW), Hooked_RegOpenKeyExW);
    }
    if (Real_RegEnumKeyExW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegEnumKeyExW), Hooked_RegEnumKeyExW);
    }
    if (Real_RegQueryValueExW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegQueryValueExW), Hooked_RegQueryValueExW);
    }
    if (Real_RegGetValueW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegGetValueW), Hooked_RegGetValueW);
    }
    if (Real_RegCloseKey) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegCloseKey), Hooked_RegCloseKey);
    }

    if (Real_RegOpenKeyExA) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegOpenKeyExA), Hooked_RegOpenKeyExA);
    }
    if (Real_RegEnumKeyExA) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegEnumKeyExA), Hooked_RegEnumKeyExA);
    }
    if (Real_RegQueryValueExA) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegQueryValueExA), Hooked_RegQueryValueExA);
    }
    if (Real_RegGetValueA) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&Real_RegGetValueA), Hooked_RegGetValueA);
    }
    detourError = DetourTransactionCommit();
}

// --- Unified Hook Implementations ---

LSTATUS WINAPI Hooked_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegOpenKeyExW_ForCertificates(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, Real_RegOpenKeyExW);
    if (handled) return status;

    Handle_RegOpenKeyExW_ForCurrentVersion(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, Real_RegOpenKeyExW);
    if (handled) return status;

    if (!Real_RegOpenKeyExW) return ERROR_CALL_NOT_IMPLEMENTED; 
    return Real_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS WINAPI Hooked_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegEnumKeyExW_ForCertificates(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime, status, handled);
    if (handled) return status;

    if (!Real_RegEnumKeyExW) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegEnumKeyExW(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
}

LSTATUS WINAPI Hooked_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegQueryValueExW_ForCertificates(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;

    Handle_RegQueryValueExW_ForMachineGuid(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;

    Handle_RegQueryValueExW_ForCurrentVersion(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;
    
    if (!Real_RegQueryValueExW) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS WINAPI Hooked_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegGetValueW_ForMachineGuid(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    Handle_RegGetValueW_ForCurrentVersion(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    if (!Real_RegGetValueW) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LSTATUS WINAPI Hooked_RegCloseKey(HKEY hKey) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegCloseKey_ForCertificates(hKey, status, handled);
    if (handled) return status; 

    Handle_RegCloseKey_ForCurrentVersion(hKey, status, handled);
    if (handled) return status;

    if (!Real_RegCloseKey) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegCloseKey(hKey);
}

// --- Unified Hook Implementations (A versions) ---

LSTATUS WINAPI Hooked_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegOpenKeyExA_ForCertificates(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, Real_RegOpenKeyExA);
    if (handled) return status;

    Handle_RegOpenKeyExA_ForCurrentVersion(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, Real_RegOpenKeyExA);
    if (handled) return status;

    if (!Real_RegOpenKeyExA) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS WINAPI Hooked_RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegEnumKeyExA_ForCertificates(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime, status, handled);
    if (handled) return status;

    if (!Real_RegEnumKeyExA) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegEnumKeyExA(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
}

LSTATUS WINAPI Hooked_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegQueryValueExA_ForCertificates(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;

    Handle_RegQueryValueExA_ForMachineGuid(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;

    Handle_RegQueryValueExA_ForCurrentVersion(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;
    
    if (!Real_RegQueryValueExA) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS WINAPI Hooked_RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegGetValueA_ForMachineGuid(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    Handle_RegGetValueA_ForCurrentVersion(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    if (!Real_RegGetValueA) return ERROR_CALL_NOT_IMPLEMENTED;
    return Real_RegGetValueA(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}
