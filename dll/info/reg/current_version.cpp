#include "current_version.h"
#include <windows.h>
#include <shlwapi.h> // For PathMatchSpecW
#include <string>
#include <vector>
#include <random>
#include <chrono>   
#include <iomanip>  
#include <sstream>  
#include <cstdio>   
#include <cstdlib>  
#include <set>      

#pragma comment(lib, "Shlwapi.lib")

// --- Static configuration and data ---
static bool s_current_version_initialized_data = false;
static std::set<HKEY> g_openCurrentVersionKeyHandles; 

static BYTE g_spoofedDigitalProductId[] = { 
    0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88  
}; 
static BYTE g_spoofedDigitalProductId4[] = { 
    0xCA, 0xFE, 0xBA, 0xBE, 0xAA, 0xBB, 0xCC, 0xDD, 
    0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80  
}; 
static DWORD g_spoofedInstallDateEpoch = 0;
static DWORD g_spoofedInstallTimeEpoch = 0; 
static std::wstring g_spoofedProductId = L"XXXXX-XXXXX-XXXXX-XXXXX-XXXXX";


static PFN_CV_RegOpenKeyExW g_true_original_RegOpenKeyExW_cv_ptr = nullptr;


// --- Helper Functions ---

DWORD GenerateRandomEpochTimestamp(int start_year, int end_year) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> year_dist(start_year, end_year);
    std::uniform_int_distribution<> month_dist(1, 12);
    std::uniform_int_distribution<> day_dist(1, 28); 

    tm t{};
    t.tm_year = year_dist(gen) - 1900; 
    t.tm_mon = month_dist(gen) - 1;    
    t.tm_mday = day_dist(gen);         
    
    std::uniform_int_distribution<> hour_dist(0, 23);
    std::uniform_int_distribution<> min_sec_dist(0, 59);
    t.tm_hour = hour_dist(gen);
    t.tm_min = min_sec_dist(gen);
    t.tm_sec = min_sec_dist(gen);
    t.tm_isdst = -1; 

    std::time_t time_since_epoch = mktime(&t);
    if (time_since_epoch == -1) {
        OutputDebugStringW(L"[CurrentVersion] mktime failed, using fallback epoch.");
        tm fallback_tm{};
        fallback_tm.tm_year = 2023 - 1900;
        fallback_tm.tm_mon = 0; 
        fallback_tm.tm_mday = 1;
        time_since_epoch = mktime(&fallback_tm);
    }
    return static_cast<DWORD>(time_since_epoch);
}

std::wstring GenerateRandomProductId() {
    std::wstring product_id;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib_char_num(0, 35); 

    auto get_random_char = [&]() -> wchar_t {
        int val = distrib_char_num(gen);
        if (val < 10) return static_cast<wchar_t>(L'0' + val);
        return static_cast<wchar_t>(L'A' + (val - 10));
    };

    for (int group = 0; group < 5; ++group) {
        for (int i = 0; i < 5; ++i) {
            product_id += get_random_char();
        }
        if (group < 4) {
            product_id += L'-';
        }
    }
    return product_id;
}

static LSTATUS ReturnRegDwordData(DWORD dwData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_DWORD;
    DWORD requiredSize = sizeof(DWORD);

    if (lpData == NULL) { 
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;

    if (*lpcbData >= requiredSize) { 
        *(reinterpret_cast<DWORD*>(lpData)) = dwData;
        *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    } else { 
        *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

static LSTATUS ReturnRegSzData(const std::wstring& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
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
        *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

static LSTATUS ReturnRegBinaryData(const BYTE* dataPtr, DWORD dataSize, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_BINARY;
    if (dataSize == 0 && dataPtr == nullptr) {
         if (lpcbData) *lpcbData = 0;
         return ERROR_FILE_NOT_FOUND; 
    }

    DWORD requiredSize = dataSize;

    if (lpData == NULL) {
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;

    if (*lpcbData >= requiredSize) {
        memcpy(lpData, dataPtr, requiredSize);
        *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    } else {
        *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

// --- Initialization ---
void InitializeCurrentVersionSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW,
    PVOID pTrueOriginalRegQueryValueExW,
    PVOID pTrueOriginalRegGetValueW,
    PVOID pTrueOriginalRegCloseKey) 
{
    if (s_current_version_initialized_data) return;

    OutputDebugStringW(L"[CurrentVersion] Initializing CurrentVersion spoofing data...");

    g_true_original_RegOpenKeyExW_cv_ptr = reinterpret_cast<PFN_CV_RegOpenKeyExW>(pTrueOriginalRegOpenKeyExW);

    if (!g_true_original_RegOpenKeyExW_cv_ptr) { 
        OutputDebugStringW(L"[CurrentVersion] Warning: True original RegOpenKeyExW pointer is NULL in Data Init.");
    }

    g_spoofedInstallDateEpoch = GenerateRandomEpochTimestamp(2022, 2025);
    g_spoofedInstallTimeEpoch = GenerateRandomEpochTimestamp(2022, 2025); 
    g_spoofedProductId = GenerateRandomProductId();

    wchar_t msg[200];
    swprintf_s(msg, L"[CurrentVersion] Spoofed InstallDateEpoch: %lu, Spoofed ProductId: %s", 
               g_spoofedInstallDateEpoch, g_spoofedProductId.c_str());
    OutputDebugStringW(msg);
    
    s_current_version_initialized_data = true;
    OutputDebugStringW(L"[CurrentVersion] Initialization complete.");
}

// --- Handler Functions ---
const wchar_t* TARGET_CV_PATH = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

void Handle_RegOpenKeyExW_ForCurrentVersion(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& return_status, bool& handled,
    PFN_CV_RegOpenKeyExW original_RegOpenKeyExW_param) 
{
    handled = false;
    if (!s_current_version_initialized_data || !original_RegOpenKeyExW_param) {
        return;
    }

    bool is_hklm = (hKey == HKEY_LOCAL_MACHINE); 

    if (is_hklm && lpSubKey && PathMatchSpecW(lpSubKey, TARGET_CV_PATH)) {
        wchar_t dbgMsg[512];
        swprintf_s(dbgMsg, L"[CurrentVersion] Handle_RegOpenKeyExW: Intercepted open for %s", lpSubKey);
        OutputDebugStringW(dbgMsg);
        
        return_status = original_RegOpenKeyExW_param(hKey, lpSubKey, ulOptions, samDesired, phkResult);
        if (return_status == ERROR_SUCCESS && phkResult && *phkResult) {
            g_openCurrentVersionKeyHandles.insert(*phkResult);
            swprintf_s(dbgMsg, L"[CurrentVersion] Handle_RegOpenKeyExW: Successfully opened and tracking HKEY 0x%p for %s", *phkResult, lpSubKey);
            OutputDebugStringW(dbgMsg);
        }
        handled = true; 
        return;
    }
}

void Handle_RegQueryValueExW_ForCurrentVersion(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_current_version_initialized_data) return;

    if (g_openCurrentVersionKeyHandles.count(hKey)) {
        if (lpValueName == nullptr) { 
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }
        
        WCHAR dbgMsg[512];
        swprintf_s(dbgMsg, L"[CurrentVersion] Handle_RegQueryValueExW: Intercepted query for value \'%s\' on tracked HKEY 0x%p", lpValueName, hKey);
        OutputDebugStringW(dbgMsg);

        if (_wcsicmp(lpValueName, L"DigitalProductId") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing DigitalProductId.");
            return_status = ReturnRegBinaryData(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), lpData, lpcbData, lpType);
            handled = true;
        } else if (_wcsicmp(lpValueName, L"DigitalProductId4") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing DigitalProductId4.");
            return_status = ReturnRegBinaryData(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), lpData, lpcbData, lpType);
            handled = true;
        } else if (_wcsicmp(lpValueName, L"InstallDate") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing InstallDate.");
            return_status = ReturnRegDwordData(g_spoofedInstallDateEpoch, lpData, lpcbData, lpType);
            handled = true;
        } else if (_wcsicmp(lpValueName, L"InstallTime") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing InstallTime.");
            return_status = ReturnRegDwordData(g_spoofedInstallTimeEpoch, lpData, lpcbData, lpType);
            handled = true;
        } else if (_wcsicmp(lpValueName, L"ProductId") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing ProductId.");
            return_status = ReturnRegSzData(g_spoofedProductId, lpData, lpcbData, lpType);
            handled = true;
        }
    }
}

void Handle_RegGetValueW_ForCurrentVersion(
    HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, 
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_current_version_initialized_data) return;

    if (hKey == HKEY_LOCAL_MACHINE && lpSubKey && PathMatchSpecW(lpSubKey, TARGET_CV_PATH)) {
        if (lpValue == nullptr) { 
             return; 
        }

        WCHAR dbgMsg[512];
        swprintf_s(dbgMsg, L"[CurrentVersion] Handle_RegGetValueW: Intercepted query for path \'%s\', value \'%s\'", lpSubKey, lpValue);
        OutputDebugStringW(dbgMsg);

        if (_wcsicmp(lpValue, L"DigitalProductId") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing DigitalProductId via RegGetValueW.");
            return_status = ReturnRegBinaryData(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), static_cast<LPBYTE>(pvData), pcbData, pdwType);
            handled = true;
        } else if (_wcsicmp(lpValue, L"DigitalProductId4") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing DigitalProductId4 via RegGetValueW.");
            return_status = ReturnRegBinaryData(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), static_cast<LPBYTE>(pvData), pcbData, pdwType);
            handled = true;
        } else if (_wcsicmp(lpValue, L"InstallDate") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing InstallDate via RegGetValueW.");
            return_status = ReturnRegDwordData(g_spoofedInstallDateEpoch, static_cast<LPBYTE>(pvData), pcbData, pdwType);
            handled = true;
        } else if (_wcsicmp(lpValue, L"InstallTime") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing InstallTime via RegGetValueW.");
            return_status = ReturnRegDwordData(g_spoofedInstallTimeEpoch, static_cast<LPBYTE>(pvData), pcbData, pdwType);
            handled = true;
        } else if (_wcsicmp(lpValue, L"ProductId") == 0) {
            OutputDebugStringW(L"[CurrentVersion] Spoofing ProductId via RegGetValueW.");
            return_status = ReturnRegSzData(g_spoofedProductId, static_cast<LPBYTE>(pvData), pcbData, pdwType);
            handled = true;
        }
    }
}

void Handle_RegCloseKey_ForCurrentVersion(
    HKEY hKey,
    LSTATUS& return_status, bool& handled)
{
    handled = false;
    if (!s_current_version_initialized_data) return;

    auto it = g_openCurrentVersionKeyHandles.find(hKey);
    if (it != g_openCurrentVersionKeyHandles.end()) {
        WCHAR dbgMsg[256];
        swprintf_s(dbgMsg, L"[CurrentVersion] Handle_RegCloseKey: Untracking HKEY 0x%p", hKey);
        OutputDebugStringW(dbgMsg);
        g_openCurrentVersionKeyHandles.erase(it);
        return_status = ERROR_SUCCESS; 
    }
}
