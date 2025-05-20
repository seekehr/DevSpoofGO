#include "machine_guid.h"
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <wchar.h> // For wcscmp, wcscpy_s

static std::wstring g_spoofedMachineGuid = L"00000000-0000-0000-0000-000000000000";
static bool s_machine_guid_initialized_data = false;

// static PFN_RegQueryValueExW g_true_original_RegQueryValueExW_mg_ptr = nullptr; // Not directly used by handlers
// static PFN_RegGetValueW g_true_original_RegGetValueW_mg_ptr = nullptr; // Not directly used by handlers

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
            }
        } else {
            OutputDebugStringA("[MachineGuid] guidString is null or empty, using default spoofed GUID.");
        }
    }
}

void InitializeMachineGuid_Centralized(PVOID pTrueOriginalRegQueryValueExW, PVOID pTrueOriginalRegGetValueW) {
    if(s_machine_guid_initialized_data) return;
    s_machine_guid_initialized_data = true;
}

void Handle_RegQueryValueExW_ForMachineGuid(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    if (lpValueName && wcscmp(lpValueName, L"MachineGuid") == 0) {
        OutputDebugStringA("[MachineGuid] Handle_RegQueryValueExW: Intercepted MachineGuid query.");
        std::wstring spoofedGuid = g_spoofedMachineGuid;
        DWORD requiredSize = (DWORD)((spoofedGuid.length() + 1) * sizeof(wchar_t));

        if (lpType) *lpType = REG_SZ;

        if (lpData == NULL) { 
            if (lpcbData) *lpcbData = requiredSize;
            return_status = ERROR_SUCCESS;
        } else { 
            if (lpcbData && *lpcbData >= requiredSize) { 
                wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), spoofedGuid.c_str());
                *lpcbData = requiredSize;
                return_status = ERROR_SUCCESS;
            } else { 
                if (lpcbData) *lpcbData = requiredSize;
                return_status = ERROR_MORE_DATA;
            }
        }
        handled = true;
        return;
    }
}

void Handle_RegGetValueW_ForMachineGuid(
    HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, 
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    if (hkey == HKEY_LOCAL_MACHINE && lpSubKey && wcscmp(lpSubKey, L"SOFTWARE\\Microsoft\\Cryptography") == 0 &&
        lpValue && wcscmp(lpValue, L"MachineGuid") == 0) 
    {
        OutputDebugStringA("[MachineGuid] Handle_RegGetValueW: Intercepted MachineGuid query for specific path.");
        std::wstring spoofedGuid = g_spoofedMachineGuid;
        DWORD requiredSize = (DWORD)((spoofedGuid.length() + 1) * sizeof(wchar_t));

        if (pdwType) *pdwType = REG_SZ;

        if (pvData == NULL) { 
            if (pcbData) *pcbData = requiredSize;
            return_status = ERROR_SUCCESS;
        } else { 
            if (pcbData && *pcbData >= requiredSize) { 
                wcscpy_s(reinterpret_cast<wchar_t*>(pvData), *pcbData / sizeof(wchar_t), spoofedGuid.c_str());
                *pcbData = requiredSize;
                return_status = ERROR_SUCCESS;
            } else { 
                if (pcbData) *pcbData = requiredSize;
                if ((dwFlags & RRF_ZEROONFAILURE) && pvData && pcbData && *pcbData > 0) {
                     memset(pvData, 0, *pcbData); 
                }
                return_status = ERROR_MORE_DATA;
            }
        }
        handled = true;
        return;
    }
}
