#include "current_version.h"
#include <windows.h>
#include <shlwapi.h> 
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

static bool s_current_version_initialized_data = false;
static std::set<HKEY> g_openCurrentVersionKeyHandles;

static BYTE g_spoofedDigitalProductId[] = {0xDE,0xAD,0xBE,0xEF,0x01,0x02,0x03,0x04,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
static BYTE g_spoofedDigitalProductId4[] = {0xCA,0xFE,0xBA,0xBE,0xAA,0xBB,0xCC,0xDD,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80};
static DWORD g_spoofedInstallDateEpoch = 0;
static DWORD g_spoofedInstallTimeEpoch = 0;
static std::wstring g_spoofedProductId = L"XXXXX-XXXXX-XXXXX-XXXXX-XXXXX";

static PFN_CV_RegOpenKeyExW g_true_original_RegOpenKeyExW_cv_ptr = nullptr;

static DWORD GenerateRandomEpochTimestamp(int start_year, int end_year) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> year_dist(start_year, end_year);
    std::uniform_int_distribution<> month_dist(1, 12);
    std::uniform_int_distribution<> day_dist(1, 28);
    tm t{};
    t.tm_year = year_dist(gen) - 1900; t.tm_mon = month_dist(gen) - 1; t.tm_mday = day_dist(gen);
    std::uniform_int_distribution<> hour_dist(0, 23);
    std::uniform_int_distribution<> min_sec_dist(0, 59);
    t.tm_hour = hour_dist(gen); t.tm_min = min_sec_dist(gen); t.tm_sec = min_sec_dist(gen);
    t.tm_isdst = -1;
    std::time_t time_since_epoch = mktime(&t);
    if (time_since_epoch == -1) {
        OutputDebugStringW(L"[CurrentVersion] mktime failed, using fallback epoch.");
        tm fallback_tm{}; fallback_tm.tm_year = 2023 - 1900; fallback_tm.tm_mon = 0; fallback_tm.tm_mday = 1;
        time_since_epoch = mktime(&fallback_tm);
    }
    return static_cast<DWORD>(time_since_epoch);
}

static std::wstring GenerateRandomProductIdW() {
    std::wstring product_id;
    std::random_device rd; std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib_char_num(0, 35);
    auto get_random_char = [&]() -> wchar_t {
        int val = distrib_char_num(gen);
        if (val < 10) return static_cast<wchar_t>(L'0' + val);
        return static_cast<wchar_t>(L'A' + (val - 10));
    };
    for (int group = 0; group < 5; ++group) {
        for (int i = 0; i < 5; ++i) product_id += get_random_char();
        if (group < 4) product_id += L'-';
    }
    return product_id;
}

static LSTATUS ReturnRegDwordDataW(DWORD dwData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_DWORD;
    DWORD requiredSize = sizeof(DWORD);
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { *(reinterpret_cast<DWORD*>(lpData)) = dwData; *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { if (lpcbData) *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}
static LSTATUS ReturnRegSzDataW(const std::wstring& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_SZ;
    DWORD requiredSize = static_cast<DWORD>((strData.length() + 1) * sizeof(wchar_t));
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), strData.c_str()); *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { if (lpcbData) *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}
static LSTATUS ReturnRegBinaryDataW(const BYTE* dataPtr, DWORD dataSize, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_BINARY;
    if (dataSize == 0 && dataPtr == nullptr) { if (lpcbData) *lpcbData = 0; return ERROR_FILE_NOT_FOUND; }
    DWORD requiredSize = dataSize;
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { memcpy(lpData, dataPtr, requiredSize); *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { if (lpcbData) *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}

void InitializeCurrentVersionSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW, 
    PVOID pTrueOriginalRegQueryValueExW,
    PVOID pTrueOriginalRegGetValueW,
    PVOID pTrueOriginalRegCloseKey
) 
{
    if (s_current_version_initialized_data) return;
    g_true_original_RegOpenKeyExW_cv_ptr = reinterpret_cast<PFN_CV_RegOpenKeyExW>(pTrueOriginalRegOpenKeyExW);
    if (!g_true_original_RegOpenKeyExW_cv_ptr) OutputDebugStringW(L"[CV] CRITICAL: True original RegOpenKeyExW is NULL in InitializeCurrentVersionSpoofing_Centralized");

    g_spoofedInstallDateEpoch = GenerateRandomEpochTimestamp(2000, 2023);
    g_spoofedInstallTimeEpoch = GenerateRandomEpochTimestamp(2000, 2023);
    g_spoofedProductId = GenerateRandomProductIdW();
    s_current_version_initialized_data = true;
}

const wchar_t* TARGET_CV_PATH_W = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

void Handle_RegOpenKeyExW_ForCurrentVersion(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult, LSTATUS& return_status, bool& handled, PFN_CV_RegOpenKeyExW original_param) {
    handled = false;
    if (!s_current_version_initialized_data) return;
    
    PFN_CV_RegOpenKeyExW effective_original_ptr = original_param ? original_param : g_true_original_RegOpenKeyExW_cv_ptr;

    if (!effective_original_ptr) {
        OutputDebugStringW(L"[CV] CRITICAL: Handle_RegOpenKeyExW effective_original_ptr is NULL.");
        return_status = ERROR_CALL_NOT_IMPLEMENTED;
        handled = true;
        return;
    }

    if (lpSubKey) {
        BOOL pathMatchResult = PathMatchSpecW(lpSubKey, TARGET_CV_PATH_W);
        bool isHKLM = (((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF));
        if (isHKLM && pathMatchResult) {
            return_status = effective_original_ptr(hKey, lpSubKey, ulOptions, samDesired, phkResult);
            if (return_status == ERROR_SUCCESS && phkResult && *phkResult) {
                g_openCurrentVersionKeyHandles.insert(*phkResult);
            }
            handled = true;
        }
    }
}
void Handle_RegQueryValueExW_ForCurrentVersion(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData, LSTATUS& return_status, bool& handled) {
    handled = false;
    if (!s_current_version_initialized_data) return;

    bool hKeyFound = g_openCurrentVersionKeyHandles.count(hKey) > 0;
    
    if (hKeyFound) {
        if (lpValueName == nullptr) {
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }
        if (_wcsicmp(lpValueName, L"DigitalProductId") == 0) { return_status = ReturnRegBinaryDataW(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"DigitalProductId4") == 0) { return_status = ReturnRegBinaryDataW(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"InstallDate") == 0) { return_status = ReturnRegDwordDataW(g_spoofedInstallDateEpoch, lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"InstallTime") == 0) { return_status = ReturnRegDwordDataW(g_spoofedInstallTimeEpoch, lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"ProductId") == 0) { return_status = ReturnRegSzDataW(g_spoofedProductId, lpData, lpcbData, lpType); handled = true; }
    }
}
void Handle_RegGetValueW_ForCurrentVersion(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData, LSTATUS& return_status, bool& handled) {
    handled = false; 
    if (!s_current_version_initialized_data) return;

    bool isHKLM_GetValue = (((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF));

    if (isHKLM_GetValue && lpSubKey && PathMatchSpecW(lpSubKey, TARGET_CV_PATH_W)) {
        if (lpValue == nullptr) {
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }

        if (_wcsicmp(lpValue, L"DigitalProductId") == 0) { 
            return_status = ReturnRegBinaryDataW(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType); handled = true; 
        }
        else if (_wcsicmp(lpValue, L"DigitalProductId4") == 0) { 
            return_status = ReturnRegBinaryDataW(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType); handled = true; 
        }
        else if (_wcsicmp(lpValue, L"InstallDate") == 0) { 
            return_status = ReturnRegDwordDataW(g_spoofedInstallDateEpoch, reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType); handled = true; 
        }
        else if (_wcsicmp(lpValue, L"InstallTime") == 0) { 
            return_status = ReturnRegDwordDataW(g_spoofedInstallTimeEpoch, reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType); handled = true; 
        }
        else if (_wcsicmp(lpValue, L"ProductId") == 0) { 
            return_status = ReturnRegSzDataW(g_spoofedProductId, reinterpret_cast<LPBYTE>(pvData), pcbData, pdwType); handled = true; 
        }
    }
}

void Handle_RegCloseKey_ForCurrentVersion(HKEY hKey, LSTATUS& return_status, bool& handled) {
    handled = false;
    if (!s_current_version_initialized_data) return;

    if (g_openCurrentVersionKeyHandles.count(hKey)) {
        g_openCurrentVersionKeyHandles.erase(hKey);
        handled = false; 
    }
    return_status = ERROR_SUCCESS; 
}
