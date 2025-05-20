#include "reg_info.h"
#include "certificates_info.h" // Corrected path
#include "machine_guid.h"      // Corrected path
#include <cstdio>              

static LSTATUS (WINAPI *TargetAPI_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY) = nullptr;
static LSTATUS (WINAPI *TargetAPI_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME) = nullptr;
static LSTATUS (WINAPI *TargetAPI_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
static LSTATUS (WINAPI *TargetAPI_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;
static LSTATUS (WINAPI *TargetAPI_RegCloseKey)(HKEY) = nullptr;

bool InitializeRegistryHooksAndHandlers() {
    OutputDebugStringW(L"REG_INFO: Initializing registry hooks and handlers...");
    HMODULE hAdvapi32 = GetModuleHandleW(L"advapi32.dll");
    if (!hAdvapi32) {
        hAdvapi32 = LoadLibraryW(L"advapi32.dll");
        if (!hAdvapi32) {
            OutputDebugStringW(L"REG_INFO: CRITICAL - Failed to load advapi32.dll.");
            return false;
        }
    }

    // Store addresses of REAL Windows APIs. Detours will place trampolines into these variables.
    TargetAPI_RegOpenKeyExW = (decltype(TargetAPI_RegOpenKeyExW))GetProcAddress(hAdvapi32, "RegOpenKeyExW");
    TargetAPI_RegEnumKeyExW = (decltype(TargetAPI_RegEnumKeyExW))GetProcAddress(hAdvapi32, "RegEnumKeyExW");
    TargetAPI_RegQueryValueExW = (decltype(TargetAPI_RegQueryValueExW))GetProcAddress(hAdvapi32, "RegQueryValueExW");
    TargetAPI_RegGetValueW = (decltype(TargetAPI_RegGetValueW))GetProcAddress(hAdvapi32, "RegGetValueW");
    TargetAPI_RegCloseKey = (decltype(TargetAPI_RegCloseKey))GetProcAddress(hAdvapi32, "RegCloseKey");

    bool success = TargetAPI_RegOpenKeyExW && TargetAPI_RegEnumKeyExW && TargetAPI_RegQueryValueExW && 
                   TargetAPI_RegGetValueW && TargetAPI_RegCloseKey;

    if (!success) {
        OutputDebugStringW(L"REG_INFO: Failed to get addresses for one or more registry functions.");
        // Continue, Centralized will also check and log if specific pointers are null.
    }

  
    InitializeCertificateSpoofing_Centralized(
        reinterpret_cast<PVOID>(TargetAPI_RegOpenKeyExW),
        reinterpret_cast<PVOID>(TargetAPI_RegEnumKeyExW),
        reinterpret_cast<PVOID>(TargetAPI_RegQueryValueExW),
        reinterpret_cast<PVOID>(TargetAPI_RegCloseKey)
    );
    InitializeMachineGuid_Centralized(
        reinterpret_cast<PVOID>(TargetAPI_RegQueryValueExW),
        reinterpret_cast<PVOID>(TargetAPI_RegGetValueW)
    );

    OutputDebugStringW(L"REG_INFO: Registry hooks and handlers initialization attempt finished.");
    return success; // Return true if all critical OS API pointers were obtained.
}

void RemoveRegistryHooksAndHandlers() {
    OutputDebugStringW(L"REG_INFO: Removing registry hooks and handlers.");
    TargetAPI_RegOpenKeyExW = nullptr;
    TargetAPI_RegEnumKeyExW = nullptr;
    TargetAPI_RegQueryValueExW = nullptr;
    TargetAPI_RegGetValueW = nullptr;
    TargetAPI_RegCloseKey = nullptr;
}

// --- Unified Hook Implementations ---

LSTATUS WINAPI Unified_Hooked_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    WCHAR debugMsg[512];
    swprintf_s(debugMsg, L"[REG_HOOK] Unified_Hooked_RegOpenKeyExW called for key: %s", lpSubKey ? lpSubKey : L"<null>");
    OutputDebugStringW(debugMsg);
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegOpenKeyExW_ForCertificates(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, TargetAPI_RegOpenKeyExW);
    if (handled) return status;

    if (!TargetAPI_RegOpenKeyExW) return ERROR_CALL_NOT_IMPLEMENTED; 
    return TargetAPI_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS WINAPI Unified_Hooked_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime) {
    WCHAR debugMsg[512];
    swprintf_s(debugMsg, L"[REG_HOOK] Unified_Hooked_RegEnumKeyExW called for key: %s", lpName ? lpName : L"<null>");
    OutputDebugStringW(debugMsg);
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegEnumKeyExW_ForCertificates(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime, status, handled);
    if (handled) return status;

    if (!TargetAPI_RegEnumKeyExW) return ERROR_CALL_NOT_IMPLEMENTED;
    return TargetAPI_RegEnumKeyExW(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
}

LSTATUS WINAPI Unified_Hooked_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    WCHAR debugMsg[512];
    swprintf_s(debugMsg, L"[REG_HOOK] Unified_Hooked_RegQueryValueExW called for key: %s", lpValueName ? lpValueName : L"<null>");
    OutputDebugStringW(debugMsg);
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegQueryValueExW_ForCertificates(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;

    Handle_RegQueryValueExW_ForMachineGuid(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData, status, handled);
    if (handled) return status;
    
    if (!TargetAPI_RegQueryValueExW) return ERROR_CALL_NOT_IMPLEMENTED;
    return TargetAPI_RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS WINAPI Unified_Hooked_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
     WCHAR debugMsg[512];
    swprintf_s(debugMsg, L"[REG_HOOK] Unified_Hooked_RegQueryValueExW called for key: %s", lpSubKey ? lpSubKey : L"<null>");
    OutputDebugStringW(debugMsg);
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    // No certificate handler for RegGetValueW currently
    Handle_RegGetValueW_ForMachineGuid(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    if (!TargetAPI_RegGetValueW) return ERROR_CALL_NOT_IMPLEMENTED;
    return TargetAPI_RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LSTATUS WINAPI Unified_Hooked_RegCloseKey(HKEY hKey) {
    OutputDebugStringW(L"[REG_HOOK] Unified_Hooked_RegCloseKey called.");
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegCloseKey_ForCertificates(hKey, status, handled);
    // If certificate handler managed the close (e.g. HKEY_SPOOFED_CERT or internal tracking), 
    // it sets status and handled. We return that. It means the key might not be closed with OS if spoofed.
    if (handled) return status; 

    // No MachineGuid handler for RegCloseKey
    if (!TargetAPI_RegCloseKey) return ERROR_CALL_NOT_IMPLEMENTED;
    return TargetAPI_RegCloseKey(hKey);
}

PVOID* GetOriginal_RegOpenKeyExW() { return reinterpret_cast<PVOID*>(&TargetAPI_RegOpenKeyExW); }
PVOID* GetOriginal_RegEnumKeyExW() { return reinterpret_cast<PVOID*>(&TargetAPI_RegEnumKeyExW); }
PVOID* GetOriginal_RegQueryValueExW() { return reinterpret_cast<PVOID*>(&TargetAPI_RegQueryValueExW); }
PVOID* GetOriginal_RegGetValueW() { return reinterpret_cast<PVOID*>(&TargetAPI_RegGetValueW); }
PVOID* GetOriginal_RegCloseKey() { return reinterpret_cast<PVOID*>(&TargetAPI_RegCloseKey); }
