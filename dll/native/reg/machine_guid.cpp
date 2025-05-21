#include "machine_guid.h"
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <wchar.h> 

static std::wstring g_spoofedMachineGuid = L"00000000-0000-0000-0000-000000000000";
static bool s_machine_guid_initialized_data = false;

// Original function pointers are not stored in this module as handlers don't call them directly.

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

// reg_info.cpp will handle A/W conversions before calling these W-only handlers.

// WIDE CHAR HELPER (for REG_SZ) 
static LSTATUS ReturnRegSzDataW(const std::wstring& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_SZ;
    DWORD requiredSize = static_cast<DWORD>((strData.length() + 1) * sizeof(wchar_t));
    if (lpData == NULL) { 
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { 
        wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), strData.c_str());
        *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    } else { 
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

void InitializeMachineGuid_Centralized(
    PVOID pTrueOriginalRegQueryValueExW, 
    PVOID pTrueOriginalRegGetValueW     
)
{
    if(s_machine_guid_initialized_data) return;
    s_machine_guid_initialized_data = true;
    OutputDebugStringA("[MachineGuid] Centralized data initialized.");
}

// W version handlers (sole handlers now)
void Handle_RegQueryValueExW_ForMachineGuid(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    if (lpValueName && wcscmp(lpValueName, L"MachineGuid") == 0) {
        // This is a common scenario where MachineGuid is read directly if the key is already open.
        // Check if hKey is HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Cryptography - this requires hKey to be the opened key to that path.
        // For simplicity and focusing on the A/W conversion, we'll assume this handler is called appropriately.
        return_status = ReturnRegSzDataW(g_spoofedMachineGuid, lpData, lpcbData, lpType);
        handled = true;
        return;
    }
}

void Handle_RegGetValueW_ForMachineGuid(
    HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags,
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    // RegGetValueW combines Open, Query, and Close. Check path here.
    if (hKey == HKEY_LOCAL_MACHINE && lpSubKey && wcscmp(lpSubKey, L"SOFTWARE\\Microsoft\\Cryptography") == 0 &&
        lpValue && wcscmp(lpValue, L"MachineGuid") == 0) 
    {
        return_status = ReturnRegSzDataW(g_spoofedMachineGuid, reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType);
        // RRF_ZEROONFAILURE is handled by the caller (Real_RegGetValueW) if we return ERROR_MORE_DATA.
        // If we fully spoof and return ERROR_SUCCESS, and the buffer was too small for RegGetValueW's sizing query, then this applies.
        // However, our ReturnRegSzDataW already handles ERROR_MORE_DATA for sizing.
        // If an application uses RRF_ZEROONFAILURE and pvData is not NULL but *pcbData is too small,
        // the OS's RegGetValueW would zero the buffer on ERROR_MORE_DATA.
        // Our helper should mimic this if we were to fully replace the original call logic for this flag.
        // For now, standard ERROR_MORE_DATA is returned by ReturnRegSzDataW.
        handled = true;
        return;
    }
}
