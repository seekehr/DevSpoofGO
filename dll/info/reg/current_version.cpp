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
static PFN_CV_RegOpenKeyExA g_true_original_RegOpenKeyExA_cv_ptr = nullptr;

static std::wstring ConvertAnsiToWide_CV(LPCSTR ansiStr) {
    if (!ansiStr) return L"";
    int lenA = lstrlenA(ansiStr);
    if (lenA == 0) return L"";
    int lenW = MultiByteToWideChar(CP_ACP, 0, ansiStr, lenA, NULL, 0);
    if (lenW == 0) return L"";
    std::wstring wideStr(lenW, 0);
    MultiByteToWideChar(CP_ACP, 0, ansiStr, lenA, &wideStr[0], lenW);
    return wideStr;
}

static std::string ConvertWideToAnsi_CV(LPCWSTR wideStr) {
    if (!wideStr) return "";
    int lenW = lstrlenW(wideStr);
    if (lenW == 0) return "";
    int lenA = WideCharToMultiByte(CP_ACP, 0, wideStr, lenW, NULL, 0, NULL, NULL);
    if (lenA == 0) return "";
    std::string ansiStr(lenA, 0);
    WideCharToMultiByte(CP_ACP, 0, wideStr, lenW, &ansiStr[0], lenA, NULL, NULL);
    return ansiStr;
}

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

static LSTATUS ReturnRegDwordDataW_CV(DWORD dwData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_DWORD;
    DWORD requiredSize = sizeof(DWORD);
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { *(reinterpret_cast<DWORD*>(lpData)) = dwData; *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}
static LSTATUS ReturnRegSzDataW_CV(const std::wstring& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_SZ;
    DWORD requiredSize = static_cast<DWORD>((strData.length() + 1) * sizeof(wchar_t));
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), strData.c_str()); *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}
static LSTATUS ReturnRegBinaryDataW_CV(const BYTE* dataPtr, DWORD dataSize, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_BINARY;
    if (dataSize == 0 && dataPtr == nullptr) { if (lpcbData) *lpcbData = 0; return ERROR_FILE_NOT_FOUND; }
    DWORD requiredSize = dataSize;
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { memcpy(lpData, dataPtr, requiredSize); *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}

static LSTATUS ReturnRegDwordDataA_CV(DWORD dwData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    return ReturnRegDwordDataW_CV(dwData, lpData, lpcbData, lpType);
}
static LSTATUS ReturnRegSzDataA_CV(const std::string& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_SZ;
    DWORD requiredSize = static_cast<DWORD>((strData.length() + 1) * sizeof(char));
    if (lpData == NULL) { if (lpcbData) *lpcbData = requiredSize; return ERROR_SUCCESS; }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;
    if (*lpcbData >= requiredSize) { strcpy_s(reinterpret_cast<char*>(lpData), *lpcbData, strData.c_str()); *lpcbData = requiredSize; return ERROR_SUCCESS; }
    else { *lpcbData = requiredSize; return ERROR_MORE_DATA; }
}
static LSTATUS ReturnRegBinaryDataA_CV(const BYTE* dataPtr, DWORD dataSize, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    return ReturnRegBinaryDataW_CV(dataPtr, dataSize, lpData, lpcbData, lpType);
}

void InitializeCurrentVersionSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW, PVOID pTrueOriginalRegQueryValueExW, PVOID pTrueOriginalRegGetValueW, PVOID pTrueOriginalRegCloseKey,
    PVOID pTrueOriginalRegOpenKeyExA, PVOID pTrueOriginalRegQueryValueExA, PVOID pTrueOriginalRegGetValueA) 
{
    if (s_current_version_initialized_data) return;
    OutputDebugStringW(L"[CV] Init CurrentVersion spoofing (A/W)...");
    g_true_original_RegOpenKeyExW_cv_ptr = reinterpret_cast<PFN_CV_RegOpenKeyExW>(pTrueOriginalRegOpenKeyExW);
    g_true_original_RegOpenKeyExA_cv_ptr = reinterpret_cast<PFN_CV_RegOpenKeyExA>(pTrueOriginalRegOpenKeyExA);
    if (!g_true_original_RegOpenKeyExW_cv_ptr) OutputDebugStringW(L"[CV] Warn: True orig RegOpenKeyExW NULL");
    if (!g_true_original_RegOpenKeyExA_cv_ptr) OutputDebugStringA("[CV] Warn: True orig RegOpenKeyExA NULL");

    g_spoofedInstallDateEpoch = GenerateRandomEpochTimestamp(2024, 2024);
    g_spoofedInstallTimeEpoch = GenerateRandomEpochTimestamp(2024, 2024);
    g_spoofedProductId = GenerateRandomProductIdW();
    wchar_t msg[200];
    swprintf_s(msg, L"[CV] Spoofed IEpoch: %lu, ProdId: %s", g_spoofedInstallDateEpoch, g_spoofedProductId.c_str());
    OutputDebugStringW(msg);
    s_current_version_initialized_data = true;
    OutputDebugStringW(L"[CV] Init complete (A/W).");
}

const wchar_t* TARGET_CV_PATH_W = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
const char*    TARGET_CV_PATH_A =  "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

void Handle_RegOpenKeyExW_ForCurrentVersion(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult, LSTATUS& return_status, bool& handled, PFN_CV_RegOpenKeyExW original_param) {
    handled = false;
    if (!s_current_version_initialized_data) {
        OutputDebugStringW(L"[CV] Handle_RegOpenKeyExW: Bailed: not initialized.");
        return;
    }
    if (!original_param) {
        OutputDebugStringW(L"[CV] Handle_RegOpenKeyExW: Bailed: original_param is NULL.");
        return;
    }

    WCHAR debugMsg[512];
    swprintf_s(debugMsg, L"[CV] Handle_RegOpenKeyExW: Checking. hKey=0x%p, lpSubKey=\'%s\'", (void*)hKey, lpSubKey ? lpSubKey : L"<null>");
    OutputDebugStringW(debugMsg);

    if (lpSubKey) {
        BOOL pathMatchResult = PathMatchSpecW(lpSubKey, TARGET_CV_PATH_W);
        swprintf_s(debugMsg, L"[CV] Handle_RegOpenKeyExW: PathMatchSpecW with \'%s\' vs TARGET_CV_PATH_W (\'%s\') -> %s",
                   lpSubKey, TARGET_CV_PATH_W, pathMatchResult ? L"TRUE" : L"FALSE");
        OutputDebugStringW(debugMsg);

        // Compare HKEYs as ULONG_PTR to avoid potential type issues, masking to lower 32 bits
        bool isHKLM = (((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF));
        if (isHKLM && pathMatchResult) {
            OutputDebugStringW(L"[CV] Handle_RegOpenKeyExW: Intercepted (Condition Met).");
            return_status = original_param(hKey, lpSubKey, ulOptions, samDesired, phkResult);
            swprintf_s(debugMsg, L"[CV] Handle_RegOpenKeyExW: Original call status: %ld. phkResult points to: 0x%p", return_status, (phkResult && *phkResult) ? (void*)*phkResult : NULL);
            OutputDebugStringW(debugMsg);
            if (return_status == ERROR_SUCCESS && phkResult && *phkResult) {
                g_openCurrentVersionKeyHandles.insert(*phkResult);
                swprintf_s(debugMsg, L"[CV] Handle_RegOpenKeyExW: Inserted HKEY 0x%p into g_openCurrentVersionKeyHandles. Set size: %zu", (void*)*phkResult, g_openCurrentVersionKeyHandles.size());
                OutputDebugStringW(debugMsg);
            }
            handled = true;
        } else {
            swprintf_s(debugMsg, L"[CV] Handle_RegOpenKeyExW: Did not intercept. ((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKLM & 0xFFFFFFFF) -> %s. pathMatch -> %s. Raw hKey: 0x%p, HKLM_masked: 0x%IX, hKey_masked: 0x%IX",
                       isHKLM ? L"TRUE" : L"FALSE",
                       pathMatchResult ? L"TRUE" : L"FALSE", (void*)hKey, (ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF, (ULONG_PTR)hKey & 0xFFFFFFFF);
            OutputDebugStringW(debugMsg);
        }
    } else {
        OutputDebugStringW(L"[CV] Handle_RegOpenKeyExW: lpSubKey is NULL, not intercepting.");
    }
}
void Handle_RegQueryValueExW_ForCurrentVersion(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData, LSTATUS& return_status, bool& handled) {
    handled = false;
    if (!s_current_version_initialized_data) {
        OutputDebugStringW(L"[CV] QueryValueExW: Bailed: not initialized.");
        return;
    }

    WCHAR debugMsg[512];
    swprintf_s(debugMsg, L"[CV] QueryValueExW: Checking. hKey=0x%p, lpValueName=\'%s\'", (void*)hKey, lpValueName ? lpValueName : L"<null>");
    OutputDebugStringW(debugMsg);

    bool hKeyFound = g_openCurrentVersionKeyHandles.count(hKey) > 0;
    swprintf_s(debugMsg, L"[CV] QueryValueExW: hKey 0x%p in g_openCurrentVersionKeyHandles? %s. Set size: %zu", (void*)hKey, hKeyFound ? L"YES" : L"NO", g_openCurrentVersionKeyHandles.size());
    OutputDebugStringW(debugMsg);
    
    if (hKeyFound) {
        if (lpValueName == nullptr) {
            OutputDebugStringW(L"[CV] QueryValueExW: lpValueName is NULL.");
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }
        OutputDebugStringW((L"[CV] QueryValueExW for: " + std::wstring(lpValueName) + L" (Condition Met)").c_str());
        if (_wcsicmp(lpValueName, L"DigitalProductId") == 0) { OutputDebugStringW(L"[CV] Spoofing DigitalProductId."); return_status = ReturnRegBinaryDataW_CV(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"DigitalProductId4") == 0) { OutputDebugStringW(L"[CV] Spoofing DigitalProductId4."); return_status = ReturnRegBinaryDataW_CV(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"InstallDate") == 0) { OutputDebugStringW(L"[CV] Spoofing InstallDate."); return_status = ReturnRegDwordDataW_CV(g_spoofedInstallDateEpoch, lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"InstallTime") == 0) { OutputDebugStringW(L"[CV] Spoofing InstallTime."); return_status = ReturnRegDwordDataW_CV(g_spoofedInstallTimeEpoch, lpData, lpcbData, lpType); handled = true; }
        else if (_wcsicmp(lpValueName, L"ProductId") == 0) { OutputDebugStringW(L"[CV] Spoofing ProductId."); return_status = ReturnRegSzDataW_CV(g_spoofedProductId, lpData, lpcbData, lpType); handled = true; }
        else { OutputDebugStringW((L"[CV] QueryValueExW: No spoof for value: " + std::wstring(lpValueName)).c_str()); }
    } else {
        OutputDebugStringW(L"[CV] QueryValueExW: HKEY not tracked, not spoofing.");
    }
}
void Handle_RegGetValueW_ForCurrentVersion(HKEY hKey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData, LSTATUS& return_status, bool& handled) {
    handled = false; if (!s_current_version_initialized_data) return;
    bool isHKLM_GetValue = (((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF));

    WCHAR debugMsg[512];

    if (isHKLM_GetValue && lpSubKey && PathMatchSpecW(lpSubKey, TARGET_CV_PATH_W)) {
        if (lpValue == nullptr) {
            OutputDebugStringW(L"[CV] GetValueW: lpValue is NULL.");
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }
        swprintf_s(debugMsg, L"[CV] GetValueW for subkey '%s', value '%s' (Condition Met)", lpSubKey, lpValue);
        OutputDebugStringW(debugMsg);

        if (_wcsicmp(lpValue, L"DigitalProductId") == 0) { 
            OutputDebugStringW(L"[CV] GetValueW: Spoofing DigitalProductId.");
            return_status = ReturnRegBinaryDataW_CV(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_wcsicmp(lpValue, L"DigitalProductId4") == 0) { 
            OutputDebugStringW(L"[CV] GetValueW: Spoofing DigitalProductId4.");
            return_status = ReturnRegBinaryDataW_CV(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_wcsicmp(lpValue, L"InstallDate") == 0) { 
            OutputDebugStringW(L"[CV] GetValueW: Spoofing InstallDate.");
            return_status = ReturnRegDwordDataW_CV(g_spoofedInstallDateEpoch, static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_wcsicmp(lpValue, L"InstallTime") == 0) { 
            OutputDebugStringW(L"[CV] GetValueW: Spoofing InstallTime.");
            return_status = ReturnRegDwordDataW_CV(g_spoofedInstallTimeEpoch, static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_wcsicmp(lpValue, L"ProductId") == 0) { 
            OutputDebugStringW(L"[CV] GetValueW: Spoofing ProductId.");
            return_status = ReturnRegSzDataW_CV(g_spoofedProductId, static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else {
            swprintf_s(debugMsg, L"[CV] GetValueW: No spoof for value: %s", lpValue);
            OutputDebugStringW(debugMsg);
        }

        if (handled && return_status == ERROR_MORE_DATA && (dwFlags & RRF_ZEROONFAILURE) && pvData && pcbData && *pcbData > 0) {
            OutputDebugStringW(L"[CV] GetValueW: Applying RRF_ZEROONFAILURE.");
            memset(pvData, 0, *pcbData);
        }
    } 
}

void Handle_RegOpenKeyExA_ForCurrentVersion(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult, LSTATUS& return_status, bool& handled, PFN_CV_RegOpenKeyExA original_param) {
    handled = false;
    if (!s_current_version_initialized_data) {
        OutputDebugStringA("[CV] Handle_RegOpenKeyExA: Bailed: not initialized.");
        return;
    }
    if (!original_param) {
        OutputDebugStringA("[CV] Handle_RegOpenKeyExA: Bailed: original_param is NULL.");
        return;
    }

    CHAR debugMsg[512];
    sprintf_s(debugMsg, "[CV] Handle_RegOpenKeyExA: Checking. hKey=0x%p, lpSubKey='%s'", (void*)hKey, lpSubKey ? lpSubKey : "<null>");
    OutputDebugStringA(debugMsg);

    if (lpSubKey) {
        BOOL pathMatchResult = PathMatchSpecA(lpSubKey, TARGET_CV_PATH_A);
        sprintf_s(debugMsg, "[CV] Handle_RegOpenKeyExA: PathMatchSpecA with '%s' vs TARGET_CV_PATH_A ('%s') -> %s",
                   lpSubKey, TARGET_CV_PATH_A, pathMatchResult ? "TRUE" : "FALSE");
        OutputDebugStringA(debugMsg);

        // Compare HKEYs as ULONG_PTR to avoid potential type issues, masking to lower 32 bits
        bool isHKLM = (((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF));
        if (isHKLM && pathMatchResult) {
            OutputDebugStringA("[CV] Handle_RegOpenKeyExA: Intercepted (Condition Met).");
            return_status = original_param(hKey, lpSubKey, ulOptions, samDesired, phkResult);
            sprintf_s(debugMsg, "[CV] Handle_RegOpenKeyExA: Original call status: %ld. phkResult points to: 0x%p", return_status, (phkResult && *phkResult) ? (void*)*phkResult : NULL);
            OutputDebugStringA(debugMsg);
            if (return_status == ERROR_SUCCESS && phkResult && *phkResult) {
                g_openCurrentVersionKeyHandles.insert(*phkResult);
                sprintf_s(debugMsg, "[CV] Handle_RegOpenKeyExA: Inserted HKEY 0x%p into g_openCurrentVersionKeyHandles. Set size: %zu", (void*)*phkResult, g_openCurrentVersionKeyHandles.size());
                OutputDebugStringA(debugMsg);
            }
            handled = true;
        } else {
            sprintf_s(debugMsg, "[CV] Handle_RegOpenKeyExA: Did not intercept. ((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKLM & 0xFFFFFFFF) -> %s. pathMatch -> %s. Raw hKey: 0x%p, HKLM_masked: 0x%IX, hKey_masked: 0x%IX",
                       isHKLM ? "TRUE" : "FALSE",
                       pathMatchResult ? "TRUE" : "FALSE", (void*)hKey, (ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF, (ULONG_PTR)hKey & 0xFFFFFFFF);
            OutputDebugStringA(debugMsg);
        }
    } else {
        OutputDebugStringA("[CV] Handle_RegOpenKeyExA: lpSubKey is NULL, not intercepting.");
    }
}
void Handle_RegQueryValueExA_ForCurrentVersion(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData, LSTATUS& return_status, bool& handled) {
    handled = false;
    if (!s_current_version_initialized_data) {
        OutputDebugStringA("[CV] QueryValueExA: Bailed: not initialized.");
        return;
    }

    CHAR debugMsg[512];
    sprintf_s(debugMsg, "[CV] QueryValueExA: Checking. hKey=0x%p, lpValueName='%s'", (void*)hKey, lpValueName ? lpValueName : "<null>");
    OutputDebugStringA(debugMsg);

    bool hKeyFound = g_openCurrentVersionKeyHandles.count(hKey) > 0;
    sprintf_s(debugMsg, "[CV] QueryValueExA: hKey 0x%p in g_openCurrentVersionKeyHandles? %s. Set size: %zu", (void*)hKey, hKeyFound ? "YES" : "NO", g_openCurrentVersionKeyHandles.size());
    OutputDebugStringA(debugMsg);

    if (hKeyFound) {
        if (lpValueName == nullptr) {
            OutputDebugStringA("[CV] QueryValueExA: lpValueName is NULL.");
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }
        OutputDebugStringA(std::string("[CV] QueryValueExA for: ") + (lpValueName ? lpValueName : "<null>") + " (Condition Met)");
        std::string ansiProductId = ConvertWideToAnsi_CV(g_spoofedProductId.c_str());
        if (_stricmp(lpValueName, "DigitalProductId") == 0) { 
            OutputDebugStringA("[CV] QueryValueExA: Spoofing DigitalProductId.");
            return_status = ReturnRegBinaryDataA_CV(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), lpData, lpcbData, lpType); 
            handled = true; 
        }
        else if (_stricmp(lpValueName, "DigitalProductId4") == 0) { 
            OutputDebugStringA("[CV] QueryValueExA: Spoofing DigitalProductId4.");
            return_status = ReturnRegBinaryDataA_CV(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), lpData, lpcbData, lpType); 
            handled = true; 
        }
        else if (_stricmp(lpValueName, "InstallDate") == 0) { 
            OutputDebugStringA("[CV] QueryValueExA: Spoofing InstallDate.");
            return_status = ReturnRegDwordDataA_CV(g_spoofedInstallDateEpoch, lpData, lpcbData, lpType); 
            handled = true; 
        }
        else if (_stricmp(lpValueName, "InstallTime") == 0) { 
            OutputDebugStringA("[CV] QueryValueExA: Spoofing InstallTime.");
            return_status = ReturnRegDwordDataA_CV(g_spoofedInstallTimeEpoch, lpData, lpcbData, lpType); 
            handled = true; 
        }
        else if (_stricmp(lpValueName, "ProductId") == 0) { 
            OutputDebugStringA("[CV] QueryValueExA: Spoofing ProductId.");
            return_status = ReturnRegSzDataA_CV(ansiProductId, lpData, lpcbData, lpType); 
            handled = true; 
        }
        else {
            sprintf_s(debugMsg, "[CV] QueryValueExA: No spoof for value: %s", lpValueName);
            OutputDebugStringA(debugMsg);
        }
    } else {
        OutputDebugStringA("[CV] QueryValueExA: HKEY not tracked, not spoofing.");
    }
}
void Handle_RegGetValueA_ForCurrentVersion(HKEY hKey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData, LSTATUS& return_status, bool& handled) {
    handled = false; 
    if (!s_current_version_initialized_data) {
        OutputDebugStringA("[CV] GetValueA: Bailed: not initialized.");
        return;
    }

    // Apply the same HKLM comparison fix here
    bool isHKLM_GetValueA = (((ULONG_PTR)hKey & 0xFFFFFFFF) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFF));
    CHAR debugMsg[512]; // For logging

    if (isHKLM_GetValueA && lpSubKey && PathMatchSpecA(lpSubKey, TARGET_CV_PATH_A)) {
        if (lpValue == nullptr) {
            OutputDebugStringA("[CV] GetValueA: lpValue is NULL.");
            return_status = ERROR_INVALID_PARAMETER;
            handled = true;
            return;
        }
        sprintf_s(debugMsg, "[CV] GetValueA for subkey '%s', value '%s' (Condition Met)", lpSubKey, lpValue);
        OutputDebugStringA(debugMsg);

        std::string ansiProductId = ConvertWideToAnsi_CV(g_spoofedProductId.c_str());
        if (_stricmp(lpValue, "DigitalProductId") == 0) { 
            OutputDebugStringA("[CV] GetValueA: Spoofing DigitalProductId.");
            return_status = ReturnRegBinaryDataA_CV(g_spoofedDigitalProductId, sizeof(g_spoofedDigitalProductId), static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_stricmp(lpValue, "DigitalProductId4") == 0) { 
            OutputDebugStringA("[CV] GetValueA: Spoofing DigitalProductId4.");
            return_status = ReturnRegBinaryDataA_CV(g_spoofedDigitalProductId4, sizeof(g_spoofedDigitalProductId4), static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_stricmp(lpValue, "InstallDate") == 0) { 
            OutputDebugStringA("[CV] GetValueA: Spoofing InstallDate.");
            return_status = ReturnRegDwordDataA_CV(g_spoofedInstallDateEpoch, static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_stricmp(lpValue, "InstallTime") == 0) { 
            OutputDebugStringA("[CV] GetValueA: Spoofing InstallTime.");
            return_status = ReturnRegDwordDataA_CV(g_spoofedInstallTimeEpoch, static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else if (_stricmp(lpValue, "ProductId") == 0) { 
            OutputDebugStringA("[CV] GetValueA: Spoofing ProductId.");
            return_status = ReturnRegSzDataA_CV(ansiProductId, static_cast<LPBYTE>(pvData), pcbData, pdwType); 
            handled = true; 
        }
        else {
            sprintf_s(debugMsg, "[CV] GetValueA: No spoof for value: %s", lpValue);
            OutputDebugStringA(debugMsg);
        }
        
        if (handled && return_status == ERROR_MORE_DATA && (dwFlags & RRF_ZEROONFAILURE) && pvData && pcbData && *pcbData > 0) {
            OutputDebugStringA("[CV] GetValueA: Applying RRF_ZEROONFAILURE.");
            memset(pvData, 0, *pcbData);
        }
    }
}

void Handle_RegCloseKey_ForCurrentVersion(HKEY hKey, LSTATUS& return_status, bool& handled) {
    handled = false;
    if (!s_current_version_initialized_data) return;
    auto it = g_openCurrentVersionKeyHandles.find(hKey);
    if (it != g_openCurrentVersionKeyHandles.end()) {
        OutputDebugStringW(L"[CV] Untracking HKEY in RegCloseKey.");
        g_openCurrentVersionKeyHandles.erase(it);
        return_status = ERROR_SUCCESS;
    }
}
