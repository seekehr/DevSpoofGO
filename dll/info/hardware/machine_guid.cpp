#include "machine_guid.h"
#include <windows.h>
#include <string>
#include <vector> 
#include <cstdio> 
#include <cstdlib>  
#include <wchar.h>  

static std::wstring g_spoofedMachineGuid = L"00000000-0000-0000-0000-000000000000"; // Default spoofed GUID

// Pointers to the original registry functions - initialized to nullptr
LSTATUS (WINAPI *g_original_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
LSTATUS (WINAPI *g_original_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;

extern "C" {
    __declspec(dllexport) void SetSpoofedMachineGuid(const char* guidString) {
        if (guidString && guidString[0] != '\0') {
            size_t newSize = strlen(guidString) + 1;
            std::vector<wchar_t> wcstring(newSize);
            size_t convertedChars = 0;
            mbstowcs_s(&convertedChars, wcstring.data(), newSize, guidString, _TRUNCATE);
            if (convertedChars > 0) {
                g_spoofedMachineGuid = wcstring.data();
            } else {
                OutputDebugStringA("[MachineGuid] Failed to convert guidString to wstring.");
                g_spoofedMachineGuid = L"00000000-1111-2222-3333-444444444444"; // Fallback
            }
        } else {
            g_spoofedMachineGuid = L"00000000-0000-0000-0000-000000000000"; // Default on null/empty
            OutputDebugStringA("[MachineGuid] guidString is null or empty, using default spoofed GUID.");
        }
    }
}

void InitializeMachineGuidHooks(PVOID pOriginalRegQueryValueExW, PVOID pOriginalRegGetValueW) {
    if (pOriginalRegQueryValueExW) {
        g_original_RegQueryValueExW = reinterpret_cast<decltype(g_original_RegQueryValueExW)>(pOriginalRegQueryValueExW);
    } else {
        OutputDebugStringA("[MachineGuid] Received NULL pOriginalRegQueryValueExW.");
    }

    if (pOriginalRegGetValueW) {
        g_original_RegGetValueW = reinterpret_cast<decltype(g_original_RegGetValueW)>(pOriginalRegGetValueW);
    } else {
        OutputDebugStringA("[MachineGuid] Received NULL pOriginalRegGetValueW.");
    }
}

PVOID* GetOriginalRegQueryValueExWPtr() {
    return reinterpret_cast<PVOID*>(&g_original_RegQueryValueExW);
}

PVOID* GetOriginalRegGetValueWPtr() {
    return reinterpret_cast<PVOID*>(&g_original_RegGetValueW);
}

LSTATUS WINAPI Hooked_RegQueryValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData)
{
    if (g_original_RegQueryValueExW && lpValueName && wcscmp(lpValueName, L"MachineGuid") == 0) {
        OutputDebugStringA("[MachineGuid] Hooked_RegQueryValueExW: Intercepted MachineGuid query.");
        std::wstring spoofedGuid = g_spoofedMachineGuid;
        DWORD requiredSize = (DWORD)((spoofedGuid.length() + 1) * sizeof(wchar_t));

        if (lpType) {
            *lpType = REG_SZ;
        }

        if (lpData == NULL) { // Caller is asking for size
            if (lpcbData) {
                *lpcbData = requiredSize;
            }
            return ERROR_SUCCESS;
        }

        if (lpcbData && *lpcbData >= requiredSize) { // Buffer is sufficient
            wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), spoofedGuid.c_str());
            *lpcbData = requiredSize;
            return ERROR_SUCCESS;
        } else { // Buffer too small or lpcbData is null (though latter is unlikely if lpData is not null)
            if (lpcbData) {
                *lpcbData = requiredSize;
            }
            return ERROR_MORE_DATA;
        }
    }

    if (!g_original_RegQueryValueExW) {
        OutputDebugStringA("[MachineGuid] Hooked_RegQueryValueExW: Original function pointer is null!");
        return ERROR_CALL_NOT_IMPLEMENTED; 
    }
    return g_original_RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS WINAPI Hooked_RegGetValueW(
    HKEY hkey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData)
{
    // Check for HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography\MachineGuid
    if (g_original_RegGetValueW && hkey == HKEY_LOCAL_MACHINE && lpSubKey && wcscmp(lpSubKey, L"SOFTWARE\\Microsoft\\Cryptography") == 0 &&
        lpValue && wcscmp(lpValue, L"MachineGuid") == 0) 
    {
        std::wstring spoofedGuid = g_spoofedMachineGuid;
        DWORD requiredSize = (DWORD)((spoofedGuid.length() + 1) * sizeof(wchar_t));

        // RRF_RT_REG_SZ is a flag that means any type is okay, and it will be converted to string.
        // We should ensure we're providing REG_SZ.
        // dwFlags can also specify RRF_ZEROONFAILURE - if we fail, we should zero pvData if this flag is set.

        if (pdwType) {
            *pdwType = REG_SZ;
        }

        if (pvData == NULL) { // Caller is asking for size
            if (pcbData) {
                *pcbData = requiredSize;
            }
            return ERROR_SUCCESS;
        }

        if (pcbData && *pcbData >= requiredSize) { // Buffer is sufficient
            wcscpy_s(reinterpret_cast<wchar_t*>(pvData), *pcbData / sizeof(wchar_t), spoofedGuid.c_str());
            *pcbData = requiredSize;
            return ERROR_SUCCESS;
        } else { // Buffer too small
            if (pcbData) {
                *pcbData = requiredSize;
            }
            if ((dwFlags & RRF_ZEROONFAILURE) && pvData && pcbData && *pcbData > 0) {
                 memset(pvData, 0, *pcbData); // Zero out buffer on failure if flag is set
            }
            return ERROR_MORE_DATA;
        }
    }

    if (!g_original_RegGetValueW) {
        OutputDebugStringA("[MachineGuid] Hooked_RegGetValueW: Original function pointer is null!");
        return ERROR_CALL_NOT_IMPLEMENTED; 
    }
    return g_original_RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}
