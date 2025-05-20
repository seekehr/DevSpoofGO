#include "certificates_info.h"
#include <shlwapi.h> 
#include <vector>
#include <iomanip>   
#include <sstream>  
#include <cstdio>    
#include <random> // For std::mt19937 and std::uniform_int_distribution
#include <set> // Added for std::set

#pragma comment(lib, "Shlwapi.lib")

static std::wstring g_spoofedCertThumbprint;
static std::vector<BYTE> g_spoofedCertBlob;

static const char* g_defaultCertBlobHex = "308202323232123082017B02020DFA300D06092A864886F70D010105050030819B310B3009060355040613024A50310E300C06035504081305546F6B796F3110300E060355040713074368756F2D6B753111300F060355040A13084672616E6B34444431183016060355040B130F5765624365727420537570706F7274311830160603550403130F4672616E6B344444205765622043413123302106092A864886F70D0109011614737570706F7274406672616E6B3464642E636F6D301E170D3132303832323035323635345A170D3137303832313035323635345A304A310B3009060355040613024A50310E300C06035504080C05546F6B796F3111300F060355040A0C084672616E6B3444443118301606035504030C0F7777772E6578616D706C652E636F6D305C300D06092A864886F70D0101010500034B0030480241009BFC6690798442BBAB13FD2B7BF8DE1512E5F193E3068A7BB8B1E19E26BB9501BFE730ED648502DD1569A834B006EC3F353C1E1B2B8FFA8F001BDF07C6AC53070203010001300D06092A864886F70D01010505000381810014B64CBB817933E671A4DA516FCB081D8D60ECBC18C7734759B1F22048BB61FAFC4DAD898DD121EBD5D8E5BAD6A636FD745083B60FC71DDF7DE52E817F45E09FE23E79EED73031C72072D9582E2AFE125A3445A119087C89475F4A95BE23214A5372DA2A052F2EC970F65BFAFDDFB431B2C14A9C062543A1E6B41E7F869B1640";
static const std::wstring g_spoofedCertSubject = L"CN=Microsoft Root Authority";
static const std::wstring g_spoofedCertIssuer = L"CN=Microsoft Root Authority";
static FILETIME g_spoofedCertValidFrom;
static FILETIME g_spoofedCertValidTo;

static HKEY g_hKeyRootCerts = NULL;
static std::set<HKEY> g_openCertRootKeyHandles; // Tracks all open HKEYs for ROOT\\Certificates

static PFN_RegOpenKeyExW g_true_original_RegOpenKeyExW_ptr = nullptr;

static bool s_certificate_spoofing_initialized_data = false;

// Helper to convert year, month, day to FILETIME
bool DateToFileTime(WORD year, WORD month, WORD day, FILETIME* ft) {
    SYSTEMTIME st = {0};
    st.wYear = year;
    st.wMonth = month;
    st.wDay = day;
    return SystemTimeToFileTime(&st, ft);
}

// Minimal HexToBytes converter
std::vector<BYTE> ConvertHexToBytes(const char* hexString) {
    std::vector<BYTE> bytes;
    size_t len = strlen(hexString);
    if (len % 2 != 0) { // Hex string must have an even length
        return bytes; 
    }
    for (size_t i = 0; i < len; i += 2) {
        char byteChars[3] = {hexString[i], hexString[i+1], '\0'};
        char* endptr;
        long val = strtol(byteChars, &endptr, 16);
        if (*endptr != '\0') { // Invalid hex character
            bytes.clear();
            return bytes;
        }
        bytes.push_back(static_cast<BYTE>(val));
    }
    return bytes;
}

void GenerateRandomThumbprint() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 15);
    
    std::wstringstream wss;
    for (int i = 0; i < 40; ++i) {
        wss << std::hex << distrib(gen);
    }
    g_spoofedCertThumbprint = wss.str();
    OutputDebugStringW((L"[CertSpoof] Generated Thumbprint: " + g_spoofedCertThumbprint).c_str());
}

// InitializeCertificateSpoofing_Centralized: Now primarily for initializing data and storing true original pointers.
void InitializeCertificateSpoofing_Centralized(
    PVOID pTrueOriginalRegOpenKeyExW,
    PVOID pTrueOriginalRegEnumKeyExW,
    PVOID pTrueOriginalRegQueryValueExW,
    PVOID pTrueOriginalRegCloseKey) 
{
    if (s_certificate_spoofing_initialized_data) return;

    g_true_original_RegOpenKeyExW_ptr = reinterpret_cast<PFN_RegOpenKeyExW>(pTrueOriginalRegOpenKeyExW);

    if (!g_true_original_RegOpenKeyExW_ptr) { 
        OutputDebugStringW(L"[CertSpoof] Warning: True original RegOpenKeyExW pointer is NULL in Data Init.");
    }

    DateToFileTime(1997, 1, 10, &g_spoofedCertValidFrom);
    DateToFileTime(2027, 12, 31, &g_spoofedCertValidTo);
    GenerateRandomThumbprint();
    g_spoofedCertBlob = ConvertHexToBytes(g_defaultCertBlobHex);
    if (g_spoofedCertBlob.empty() && g_defaultCertBlobHex && *g_defaultCertBlobHex) {
        OutputDebugStringW(L"[CertSpoof] Warning: Default cert blob hex failed to convert.");
    }

    s_certificate_spoofing_initialized_data = true;
}

const wchar_t* TARGET_LM_CERT_ROOT_PATH = L"SOFTWARE\\Microsoft\\SystemCertificates\\ROOT\\Certificates";

static LSTATUS ReturnRegSzData(const std::wstring& strData, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_SZ;
    DWORD requiredSize = static_cast<DWORD>((strData.length() + 1) * sizeof(wchar_t));

    if (lpData == NULL) { // Caller is asking for size
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;

    if (*lpcbData >= requiredSize) { // Buffer is sufficient
        wcscpy_s(reinterpret_cast<wchar_t*>(lpData), *lpcbData / sizeof(wchar_t), strData.c_str());
        *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    } else { // Buffer too small
        *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

static LSTATUS ReturnRegBinaryData(const BYTE* dataPtr, DWORD dataSize, LPBYTE lpData, LPDWORD lpcbData, LPDWORD lpType) {
    if (lpType) *lpType = REG_BINARY;
    if (dataSize == 0 && dataPtr == nullptr) { // Specifically for empty blob
         if (lpcbData) *lpcbData = 0;
         return ERROR_FILE_NOT_FOUND; // Or ERROR_SUCCESS if 0-byte blob is valid scenario to return for query
    }

    DWORD requiredSize = dataSize;

    if (lpData == NULL) { // Caller is asking for size
        if (lpcbData) *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    }
    if (lpcbData == NULL) return ERROR_INVALID_PARAMETER;

    if (*lpcbData >= requiredSize) { // Buffer is sufficient
        memcpy(lpData, dataPtr, requiredSize);
        *lpcbData = requiredSize;
        return ERROR_SUCCESS;
    } else { // Buffer too small
        *lpcbData = requiredSize;
        return ERROR_MORE_DATA;
    }
}

// --- New Handler Functions ---

void Handle_RegOpenKeyExW_ForCertificates(
    HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult,
    LSTATUS& return_status, bool& handled,
    PFN_RegOpenKeyExW original_RegOpenKeyExW_param) 
{
    handled = false;

    if (!s_certificate_spoofing_initialized_data) {
        return;
    }
    if (!original_RegOpenKeyExW_param) { 
        return; 
    }

    // Use masked comparison for HKEY_LOCAL_MACHINE
    if (((ULONG_PTR)hKey & 0xFFFFFFFFUL) == ((ULONG_PTR)HKEY_LOCAL_MACHINE & 0xFFFFFFFFUL) && 
        lpSubKey && PathMatchSpecW(lpSubKey, TARGET_LM_CERT_ROOT_PATH)) 
    {
        OutputDebugStringW(L"[CertSpoof] Handle_RegOpenKeyExW: Intercepted HKLM\\...\\ROOT\\Certificates open (Condition Met with MASKED HKLM comparison).");
        return_status = original_RegOpenKeyExW_param(hKey, lpSubKey, ulOptions, samDesired, phkResult);
        if (return_status == ERROR_SUCCESS && phkResult && *phkResult) {
            g_openCertRootKeyHandles.insert(*phkResult);
        }
        handled = true; 
        return;
    }

    if (g_openCertRootKeyHandles.count(hKey) &&
        lpSubKey && !g_spoofedCertThumbprint.empty() && _wcsicmp(lpSubKey, g_spoofedCertThumbprint.c_str()) == 0)
    {
        wchar_t msgBuffer[256]; 
        swprintf_s(msgBuffer, L"[CertSpoof] Handle_RegOpenKeyExW: Intercepted open.");
        OutputDebugStringW(msgBuffer);
        if (phkResult) *phkResult = HKEY_SPOOFED_CERT;
        return_status = ERROR_SUCCESS;
        handled = true;
        return;
    }
}

void Handle_RegEnumKeyExW_ForCertificates(
    HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, 
    LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime,
    LSTATUS& return_status, bool& handled)
{
    handled = false;

    if (!s_certificate_spoofing_initialized_data) {
        return;
    }

    if (g_openCertRootKeyHandles.count(hKey)) {
        if (dwIndex == 0 && !g_spoofedCertThumbprint.empty()) {
            OutputDebugStringW((L"[CertSpoof] Handle_RegEnumKeyExW: Spoofing for index 0 with thumbprint: " + g_spoofedCertThumbprint).c_str());
            DWORD thumbprintLen = static_cast<DWORD>(g_spoofedCertThumbprint.length());
            
            if (lpcchName == NULL) { return_status = ERROR_INVALID_PARAMETER; handled = true; return; }

            if (lpName != NULL && *lpcchName > thumbprintLen) {
                wcscpy_s(lpName, *lpcchName, g_spoofedCertThumbprint.c_str());
                *lpcchName = thumbprintLen;
                if (lpClass && lpcchClass && *lpcchClass > 0) { 
                    const wchar_t* certStoreClass = L"CryptPKIMgrCert"; 
                    size_t certStoreClassLen = wcslen(certStoreClass);
                    if (*lpcchClass > certStoreClassLen) {
                         wcscpy_s(lpClass, *lpcchClass, certStoreClass);
                        *lpcchClass = static_cast<DWORD>(certStoreClassLen);
                    } else { *lpcchClass = static_cast<DWORD>(certStoreClassLen + 1); }
                }
                if (lpftLastWriteTime) { DateToFileTime(2000, 1, 1, lpftLastWriteTime); }
                return_status = ERROR_SUCCESS;
            } else { 
                *lpcchName = thumbprintLen + 1; 
                return_status = ERROR_MORE_DATA;
            }
        } else {
            return_status = ERROR_NO_MORE_ITEMS; 
        }
        handled = true;
        return;
    }
}

void Handle_RegQueryValueExW_ForCertificates(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& return_status, bool& handled)
{
    handled = false;

    if (!s_certificate_spoofing_initialized_data) {
        return;
    }

    if (hKey == HKEY_SPOOFED_CERT) {
        if (lpValueName == nullptr) { 
            return_status = ERROR_INVALID_PARAMETER; 
            handled = true; 
            return; 
        }

        if (_wcsicmp(lpValueName, L"Blob") == 0) {
            OutputDebugStringW(L"[CertSpoof] Handle_RegQueryValueExW: Spoofing Blob for HKEY_SPOOFED_CERT.");
            return_status = ReturnRegBinaryData(g_spoofedCertBlob.empty() ? nullptr : g_spoofedCertBlob.data(), 
                                       static_cast<DWORD>(g_spoofedCertBlob.size()), lpData, lpcbData, lpType);
        } else if (_wcsicmp(lpValueName, L"Subject Name") == 0 || _wcsicmp(lpValueName, L"Subject") == 0) {
            OutputDebugStringW(L"[CertSpoof] Handle_RegQueryValueExW: Spoofing Subject Name for HKEY_SPOOFED_CERT.");
            return_status = ReturnRegSzData(g_spoofedCertSubject, lpData, lpcbData, lpType);
        } else if (_wcsicmp(lpValueName, L"Issuer Name") == 0 || _wcsicmp(lpValueName, L"Issuer") == 0) {
            OutputDebugStringW(L"[CertSpoof] Handle_RegQueryValueExW: Spoofing Issuer Name for HKEY_SPOOFED_CERT.");
            return_status = ReturnRegSzData(g_spoofedCertIssuer, lpData, lpcbData, lpType);
        } else if (_wcsicmp(lpValueName, L"ValidFrom") == 0 || _wcsicmp(lpValueName, L"NotBefore") == 0) {
            OutputDebugStringW(L"[CertSpoof] Handle_RegQueryValueExW: Spoofing ValidFrom for HKEY_SPOOFED_CERT.");
            return_status = ReturnRegBinaryData(reinterpret_cast<const BYTE*>(&g_spoofedCertValidFrom), 
                                       sizeof(g_spoofedCertValidFrom), lpData, lpcbData, lpType);
        } else if (_wcsicmp(lpValueName, L"ValidTo") == 0 || _wcsicmp(lpValueName, L"NotAfter") == 0) {
            OutputDebugStringW(L"[CertSpoof] Handle_RegQueryValueExW: Spoofing ValidTo for HKEY_SPOOFED_CERT.");
            return_status = ReturnRegBinaryData(reinterpret_cast<const BYTE*>(&g_spoofedCertValidTo), 
                                       sizeof(g_spoofedCertValidTo), lpData, lpcbData, lpType);
        } else {
            OutputDebugStringW((L"[CertSpoof] Handle_RegQueryValueExW: Value " + (lpValueName ? std::wstring(lpValueName) : L"(null)") + L" not found for HKEY_SPOOFED_CERT.").c_str());
            if (lpcbData) *lpcbData = 0;
            return_status = ERROR_FILE_NOT_FOUND; 
        }
        handled = true;
        return;
    }
}

void Handle_RegCloseKey_ForCertificates(
    HKEY hKey,
    LSTATUS& return_status, bool& handled)
{
    handled = false;

    if (!s_certificate_spoofing_initialized_data) {
        return;
    }

    if (hKey == HKEY_SPOOFED_CERT) {
        OutputDebugStringW(L"[CertSpoof] Handle_RegCloseKey: Intercepted close for HKEY_SPOOFED_CERT.");
        return_status = ERROR_SUCCESS; 
        handled = true;
        return;
    }
    
    auto it = g_openCertRootKeyHandles.find(hKey);
    if (it != g_openCertRootKeyHandles.end()) {
        g_openCertRootKeyHandles.erase(it);
        return_status = ERROR_SUCCESS; 
        handled = true; 
        return;
    }
}
