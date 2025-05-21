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

// Helper for ANSI string conversion (can be shared if moved to a common util header)
static std::wstring ConvertAnsiToWide_MG(LPCSTR ansiStr) { // Suffix to avoid redef if linking with others
    if (!ansiStr) return L"";
    int lenA = lstrlenA(ansiStr);
    if (lenA == 0) return L"";
    int lenW = MultiByteToWideChar(CP_ACP, 0, ansiStr, lenA, NULL, 0);
    if (lenW == 0) return L"";
    std::wstring wideStr(lenW, 0);
    MultiByteToWideChar(CP_ACP, 0, ansiStr, lenA, &wideStr[0], lenW);
    return wideStr;
}

static std::string ConvertWideToAnsi_MG(LPCWSTR wideStr) { // Suffix to avoid redef
    if (!wideStr) return "";
    int lenW = lstrlenW(wideStr);
    if (lenW == 0) return "";
    int lenA = WideCharToMultiByte(CP_ACP, 0, wideStr, lenW, NULL, 0, NULL, NULL);
    if (lenA == 0) return "";
    std::string ansiStr(lenA, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr, lenW, &ansiStr[0], lenA, NULL, NULL);
    return ansiStr;
}

// WIDE CHAR HELPER (for REG_SZ)
static LSTATUS ReturnRegSzDataW_MG(const std::wstring& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
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

// ANSI CHAR HELPER (for REG_SZ)
static LSTATUS ReturnRegSzDataA_MG(const std::string& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_SZ;
    DWORD requiredSize = static_cast<DWORD>((strData.length() + 1) * sizeof(char));
    if (lpData == NULL) {
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) {
        strcpy_s(reinterpret_cast<char*>(lpData), *lpcbData, strData.c_str());
        *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    } else {
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

void InitializeMachineGuid_Centralized(
    PVOID pTrueOriginalRegQueryValueExW, PVOID pTrueOriginalRegGetValueW,
    PVOID pTrueOriginalRegQueryValueExA, PVOID pTrueOriginalRegGetValueA)
{
    if(s_machine_guid_initialized_data) return;
    // Store original function pointers if this module needs to call them directly (currently it doesn't)
    s_machine_guid_initialized_data = true;
    OutputDebugStringA("[MachineGuid] Centralized data initialized.");
}

// W version handlers
void Handle_RegQueryValueExW_ForMachineGuid(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    if (lpValueName && wcscmp(lpValueName, L"MachineGuid") == 0) {
        OutputDebugStringA("[MachineGuid] Handle_RegQueryValueExW: Intercepted MachineGuid query.");
        return_status = ReturnRegSzDataW_MG(g_spoofedMachineGuid, lpData, lpcbData, lpType);
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
        return_status = ReturnRegSzDataW_MG(g_spoofedMachineGuid, reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType);
        // Special handling for RRF_ZEROONFAILURE based on RegGetValueW documentation
        if (return_status == ERROR_MORE_DATA && (dwFlags & RRF_ZEROONFAILURE) && pvData && pcbData && *pcbData > 0) {
            memset(pvData, 0, *pcbData); 
        }
        handled = true;
        return;
    }
}

// A version handlers
void Handle_RegQueryValueExA_ForMachineGuid(
    HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    if (lpValueName && strcmp(lpValueName, "MachineGuid") == 0) {
        OutputDebugStringA("[MachineGuid] Handle_RegQueryValueExA: Intercepted MachineGuid query.");
        std::string ansiSpoofedGuid = ConvertWideToAnsi_MG(g_spoofedMachineGuid.c_str());
        return_status = ReturnRegSzDataA_MG(ansiSpoofedGuid, lpData, lpcbData, lpType);
        handled = true;
        return;
    }
}

void Handle_RegGetValueA_ForMachineGuid(
    HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, 
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_machine_guid_initialized_data) return;

    if (hkey == HKEY_LOCAL_MACHINE && lpSubKey && strcmp(lpSubKey, "SOFTWARE\\Microsoft\\Cryptography") == 0 &&
        lpValue && strcmp(lpValue, "MachineGuid") == 0) 
    {
        OutputDebugStringA("[MachineGuid] Handle_RegGetValueA: Intercepted MachineGuid query for specific path.");
        std::string ansiSpoofedGuid = ConvertWideToAnsi_MG(g_spoofedMachineGuid.c_str());
        return_status = ReturnRegSzDataA_MG(ansiSpoofedGuid, reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType);
        if (return_status == ERROR_MORE_DATA && (dwFlags & RRF_ZEROONFAILURE) && pvData && pcbData && *pcbData > 0) {
            memset(pvData, 0, *pcbData); 
        }
        handled = true;
        return;
    }
}
