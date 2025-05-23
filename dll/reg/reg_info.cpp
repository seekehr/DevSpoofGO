#include "reg_info.h"
#include "certificates_info.h" // Corrected path
#include "machine_guid.h"      // Corrected path
#include "current_version.h"   // Added for CurrentVersion spoofing
#include <cstdio>              
#include "../../detours/include/detours.h" // Added for DetourDetach
#include <vector> // Required for std::vector
#include <string> // Required for std::wstring

LSTATUS (WINAPI *g_true_original_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY) = nullptr;
LSTATUS (WINAPI *g_true_original_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME) = nullptr;
LSTATUS (WINAPI *g_true_original_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
LSTATUS (WINAPI *g_true_original_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;
LSTATUS (WINAPI *g_true_original_RegCloseKey)(HKEY) = nullptr;

// These static pointers are used to store the result of GetProcAddress before assigning to g_true_original_...
// and are also the functions that the Hooked_Reg...A implementations will ultimately call (via g_true_original_... after detouring).
static LSTATUS (WINAPI *Real_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY) = nullptr;
static LSTATUS (WINAPI *Real_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME) = nullptr;
static LSTATUS (WINAPI *Real_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
static LSTATUS (WINAPI *Real_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;
static LSTATUS (WINAPI *Real_RegCloseKey)(HKEY) = nullptr;

// Helper function to convert ANSI to Wide string
std::wstring AnsiToWide(LPCSTR ansiStr) {
    if (!ansiStr) return L"";
    int requiredSize = MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, nullptr, 0);
    if (requiredSize == 0) return L"";
    std::wstring wideStr(requiredSize -1, L'\0'); // -1 for null terminator, corrected L'\\0' to L'\0'
    MultiByteToWideChar(CP_ACP, 0, ansiStr, -1, &wideStr[0], requiredSize);
    return wideStr;
}

// Helper function to convert Wide to ANSI string
std::string WideToAnsi(LPCWSTR wideStr) {
    if (!wideStr) return "";
    int requiredSize = WideCharToMultiByte(CP_ACP, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (requiredSize == 0) return "";
    std::string ansiStr(requiredSize - 1, '\0'); // Corrected: removed extra backslash from '\\0'
    WideCharToMultiByte(CP_ACP, 0, wideStr, -1, &ansiStr[0], requiredSize, nullptr, nullptr);
    return ansiStr;
}

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

    bool success = Real_RegOpenKeyExW && Real_RegEnumKeyExW && Real_RegQueryValueExW && 
                   Real_RegGetValueW && Real_RegCloseKey;

    if (!success) {
        OutputDebugStringW(L"REG_INFO: CRITICAL - Failed to get addresses for one or more WIDE registry functions. Cannot proceed with Detours.");
        return false; // Cannot proceed if function pointers are null
    }
    
    // Store the true originals for Detours to use. These are extern declared in reg_info.h
    g_true_original_RegOpenKeyExW = Real_RegOpenKeyExW;
    g_true_original_RegEnumKeyExW = Real_RegEnumKeyExW;
    g_true_original_RegQueryValueExW = Real_RegQueryValueExW;
    g_true_original_RegGetValueW = Real_RegGetValueW;
    g_true_original_RegCloseKey = Real_RegCloseKey;

    InitializeCertificateSpoofing_Centralized(
        reinterpret_cast<PVOID>(Real_RegOpenKeyExW),
        reinterpret_cast<PVOID>(Real_RegEnumKeyExW),
        reinterpret_cast<PVOID>(Real_RegQueryValueExW),
        reinterpret_cast<PVOID>(Real_RegCloseKey)
    );
    InitializeMachineGuid_Centralized(
        reinterpret_cast<PVOID>(Real_RegQueryValueExW),
        reinterpret_cast<PVOID>(Real_RegGetValueW)
    );
    InitializeCurrentVersionSpoofing_Centralized(
        reinterpret_cast<PVOID>(Real_RegOpenKeyExW),
        reinterpret_cast<PVOID>(Real_RegQueryValueExW),
        reinterpret_cast<PVOID>(Real_RegGetValueW),
        reinterpret_cast<PVOID>(Real_RegCloseKey)
    );

    LONG detourError;
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // Hook W versions using the g_true_original_ pointers for Detours
    detourError = DetourAttach(reinterpret_cast<PVOID*>(&g_true_original_RegOpenKeyExW), Hooked_RegOpenKeyExW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&g_true_original_RegEnumKeyExW), Hooked_RegEnumKeyExW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&g_true_original_RegQueryValueExW), Hooked_RegQueryValueExW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&g_true_original_RegGetValueW), Hooked_RegGetValueW);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourAttach(reinterpret_cast<PVOID*>(&g_true_original_RegCloseKey), Hooked_RegCloseKey);
    if (detourError != NO_ERROR) success = false;

    detourError = DetourTransactionCommit();

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

    // Detach W versions using g_true_original_
    if (g_true_original_RegOpenKeyExW) { // Check against g_true_original which holds the trampoline
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&g_true_original_RegOpenKeyExW), Hooked_RegOpenKeyExW);
    }
    if (g_true_original_RegEnumKeyExW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&g_true_original_RegEnumKeyExW), Hooked_RegEnumKeyExW);
    }
    if (g_true_original_RegQueryValueExW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&g_true_original_RegQueryValueExW), Hooked_RegQueryValueExW);
    }
    if (g_true_original_RegGetValueW) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&g_true_original_RegGetValueW), Hooked_RegGetValueW);
    }
    if (g_true_original_RegCloseKey) {
        detourError = DetourDetach(reinterpret_cast<PVOID*>(&g_true_original_RegCloseKey), Hooked_RegCloseKey);
    }

    detourError = DetourTransactionCommit();
}

// --- Unified Hook Implementations ---

LSTATUS WINAPI Hooked_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegOpenKeyExW_ForCertificates(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, g_true_original_RegOpenKeyExW);
    if (handled) return status;

    Handle_RegOpenKeyExW_ForCurrentVersion(hKey, lpSubKey, ulOptions, samDesired, phkResult, status, handled, g_true_original_RegOpenKeyExW);
    if (handled) return status;

    if (!g_true_original_RegOpenKeyExW) return ERROR_CALL_NOT_IMPLEMENTED; 
    return g_true_original_RegOpenKeyExW(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS WINAPI Hooked_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegEnumKeyExW_ForCertificates(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime, status, handled);
    if (handled) return status;

    if (!g_true_original_RegEnumKeyExW) return ERROR_CALL_NOT_IMPLEMENTED;
    return g_true_original_RegEnumKeyExW(hKey, dwIndex, lpName, lpcchName, lpReserved, lpClass, lpcchClass, lpftLastWriteTime);
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
    
    if (!g_true_original_RegQueryValueExW) return ERROR_CALL_NOT_IMPLEMENTED;
    return g_true_original_RegQueryValueExW(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LSTATUS WINAPI Hooked_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegGetValueW_ForMachineGuid(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    Handle_RegGetValueW_ForCurrentVersion(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData, status, handled);
    if (handled) return status;

    if (!g_true_original_RegGetValueW) return ERROR_CALL_NOT_IMPLEMENTED;
    return g_true_original_RegGetValueW(hkey, lpSubKey, lpValue, dwFlags, pdwType, pvData, pcbData);
}

LSTATUS WINAPI Hooked_RegCloseKey(HKEY hKey) {
    LSTATUS status = ERROR_SUCCESS;
    bool handled = false;

    Handle_RegCloseKey_ForCertificates(hKey, status, handled);
    if (handled) return status; 

    Handle_RegCloseKey_ForCurrentVersion(hKey, status, handled);
    if (handled) return status;

    if (!g_true_original_RegCloseKey) return ERROR_CALL_NOT_IMPLEMENTED;
    return g_true_original_RegCloseKey(hKey);
}

// --- Unified Hook Implementations (A versions) ---

LSTATUS WINAPI Hooked_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    // We should call the *original* function if our hook isn't prepared to handle it.
    // g_true_original_RegOpenKeyExW is the trampoline to the original RegOpenKeyExW or the next hook.
    if (!g_true_original_RegOpenKeyExW) {
        // This case should ideally not happen if initialization was successful.
        // If it does, it means Detours didn't set up the trampoline for the W version.
        // We might try to call the original Real_RegOpenKeyExA if we had fetched it, 
        // but the design is to forward to W hooks.
        // Calling system's RegOpenKeyExA directly would bypass other potential hooks if any.
        // Safest is to return an error indicating failure.
        return ERROR_CALL_NOT_IMPLEMENTED; 
    }

    std::wstring subKeyW = AnsiToWide(lpSubKey);
    // Call the WIDE hooked version, which will then call any spoofing handlers
    // and eventually g_true_original_RegOpenKeyExW (the real RegOpenKeyExW).
    return Hooked_RegOpenKeyExW(hKey, subKeyW.c_str(), ulOptions, samDesired, phkResult);
}

LSTATUS WINAPI Hooked_RegEnumKeyExA(
    HKEY hKey,
    DWORD dwIndex,
    LPSTR lpName,
    LPDWORD lpcchName,
    LPDWORD lpReserved,
    LPSTR lpClass,
    LPDWORD lpcchClass,
    PFILETIME lpftLastWriteTime
) {
    if (!g_true_original_RegEnumKeyExW) return ERROR_CALL_NOT_IMPLEMENTED;
    if (!lpcchName) return ERROR_INVALID_PARAMETER;

    DWORD cchNameW_provided = *lpcchName; // Original A buffer size in chars
    std::vector<wchar_t> nameW(cchNameW_provided + 1); // Temp W buffer, assume 1:1 mapping for initial size, +1 for safety
    DWORD cchNameW_actual = cchNameW_provided; // Pass this to W func as available WIDE char count

    std::vector<wchar_t> classW;
    DWORD cchClassW_provided = 0;
    DWORD cchClassW_actual = 0;
    if (lpClass && lpcchClass && *lpcchClass > 0) {
        cchClassW_provided = *lpcchClass; // Original A buffer size for class
        classW.resize(cchClassW_provided + 1); // Temp W buffer for class
        cchClassW_actual = cchClassW_provided;
    }

    LSTATUS status = Hooked_RegEnumKeyExW(
        hKey,
        dwIndex,
        nameW.data(),
        &cchNameW_actual, // Receives WCHARs written (excluding null)
        lpReserved,
        (lpClass && lpcchClass && *lpcchClass > 0) ? classW.data() : nullptr,
        (lpClass && lpcchClass && *lpcchClass > 0) ? &cchClassW_actual : nullptr, // Receives WCHARs written
        lpftLastWriteTime
    );

    if (status == ERROR_SUCCESS) {
        // Convert nameW to lpName (ANSI)
        // cchNameW_actual has count of WCHARs written by RegEnumKeyExW, excluding null.
        int charsWrittenA = WideCharToMultiByte(CP_ACP, 0, nameW.data(), cchNameW_actual, lpName, *lpcchName, nullptr, nullptr);
        if (charsWrittenA == 0 && cchNameW_actual > 0) { // If conversion failed but there was data
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                *lpcchName = WideCharToMultiByte(CP_ACP, 0, nameW.data(), cchNameW_actual, nullptr, 0, nullptr, nullptr);
                return ERROR_MORE_DATA;
            }
            return ERROR_CANTOPEN; // Other conversion error
        }
        if (lpName && (DWORD)charsWrittenA < *lpcchName) { // If there's space for null
             lpName[charsWrittenA] = '\0'; // Null terminate
        }
        *lpcchName = charsWrittenA; // Number of chars written (RegEnumKeyExA expects this)

        // Convert classW to lpClass (ANSI)
        if (lpClass && lpcchClass && *lpcchClass > 0 && classW.size() > 0 && cchClassW_actual > 0) {
            charsWrittenA = WideCharToMultiByte(CP_ACP, 0, classW.data(), cchClassW_actual, lpClass, *lpcchClass, nullptr, nullptr);
            if (charsWrittenA == 0 && cchClassW_actual > 0) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    *lpcchClass = WideCharToMultiByte(CP_ACP, 0, classW.data(), cchClassW_actual, nullptr, 0, nullptr, nullptr);
                     // If name succeeded but class failed for size, return ERROR_MORE_DATA for class
                    return ERROR_MORE_DATA; 
                }
                return ERROR_CANTOPEN;
            }
            if (lpClass && (DWORD)charsWrittenA < *lpcchClass) {
                 lpClass[charsWrittenA] = '\0';
            }
            *lpcchClass = charsWrittenA;
        } else if (lpClass && lpcchClass) {
            *lpcchClass = 0; // No class to convert or output buffer was 0.
        }
    } else if (status == ERROR_MORE_DATA) {
        // Hooked_RegEnumKeyExW returned ERROR_MORE_DATA.
        // cchNameW_actual now holds the required WIDE char count for name.
        // cchClassW_actual (if class was queried) holds required WIDE char count for class.
        *lpcchName = WideCharToMultiByte(CP_ACP, 0, nameW.data(), cchNameW_actual, nullptr, 0, nullptr, nullptr);
        if (lpClass && lpcchClass && *lpcchClass > 0) {
            *lpcchClass = WideCharToMultiByte(CP_ACP, 0, classW.data(), cchClassW_actual, nullptr, 0, nullptr, nullptr);
        }
    }
    return status;
}

LSTATUS WINAPI Hooked_RegQueryValueExA(
    HKEY hKey,
    LPCSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
) {
    if (!g_true_original_RegQueryValueExW) return ERROR_CALL_NOT_IMPLEMENTED;

    std::wstring valueNameW = AnsiToWide(lpValueName);
    DWORD cbDataA_provided = 0;
    if (lpcbData) cbDataA_provided = *lpcbData; // Original A buffer size in bytes

    LSTATUS status;
    DWORD typeFromW = 0; // Store type from W call
    if (lpType) typeFromW = *lpType; // Initialize with user-provided type if any

    // If lpData is NULL, client is querying size/type.
    if (lpData == NULL) {
        status = Hooked_RegQueryValueExW(hKey, valueNameW.c_str(), lpReserved, &typeFromW, nullptr, lpcbData); // lpcbData for W will get required W-bytes
        if (lpType) *lpType = typeFromW;

        if (status == ERROR_SUCCESS || status == ERROR_MORE_DATA) { // lpcbData has W-bytes
            if (lpcbData && (typeFromW == REG_SZ || typeFromW == REG_EXPAND_SZ || typeFromW == REG_MULTI_SZ)) {
                // Need to convert required W-bytes to required A-bytes.
                // Must make a call to get the data to measure it accurately.
                if (*lpcbData > 0) { 
                    std::vector<wchar_t> tempW_data(*lpcbData / sizeof(wchar_t) + 1);
                    DWORD temp_cbDataW = *lpcbData;
                    LSTATUS temp_status = g_true_original_RegQueryValueExW(hKey, valueNameW.c_str(), lpReserved, &typeFromW, reinterpret_cast<LPBYTE>(tempW_data.data()), &temp_cbDataW);
                    if (temp_status == ERROR_SUCCESS) {
                        *lpcbData = WideCharToMultiByte(CP_ACP, 0, tempW_data.data(), temp_cbDataW / sizeof(wchar_t), nullptr, 0, nullptr, nullptr);
                    } else {
                        // Cannot determine actual A-bytes, *lpcbData is still W-bytes. This is imperfect.
                    }
                }
                // if *lpcbData is 0, then required A-bytes is also 0.
            }
            // For non-string types, or if *lpcbData is 0, lpcbData (bytes) is fine as is.
        }
        return status;
    }


    // First call W to get data into a WIDE buffer.
    // Initial guess for W buffer size: cbDataA_provided (bytes for A) can hold at most cbDataA_provided WCHARs.
    std::vector<BYTE> dataW_vec(cbDataA_provided * sizeof(wchar_t) + 2); // A bit larger to be safe for WIDE chars
    DWORD cbDataW_actual = static_cast<DWORD>(dataW_vec.size());

    status = Hooked_RegQueryValueExW(hKey, valueNameW.c_str(), lpReserved, &typeFromW, dataW_vec.data(), &cbDataW_actual);
    if (lpType) *lpType = typeFromW;

    if (status == ERROR_SUCCESS) {
        if (typeFromW == REG_SZ || typeFromW == REG_EXPAND_SZ || typeFromW == REG_MULTI_SZ) {
            // cbDataW_actual is number of WIDE bytes written, including null(s).
            int bytesWrittenA = WideCharToMultiByte(CP_ACP, 0, 
                                                  reinterpret_cast<LPCWSTR>(dataW_vec.data()), 
                                                  cbDataW_actual / sizeof(wchar_t), // Number of WCHARs
                                                  reinterpret_cast<LPSTR>(lpData), 
                                                  cbDataA_provided, // Max ANSI bytes
                                                  nullptr, nullptr);
            if (bytesWrittenA == 0 && cbDataW_actual > 0) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    *lpcbData = WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWSTR>(dataW_vec.data()), cbDataW_actual / sizeof(wchar_t), nullptr, 0, nullptr, nullptr);
                    return ERROR_MORE_DATA;
                }
                return ERROR_CANTOPEN; // Other conversion error
            }
            *lpcbData = bytesWrittenA;
        } else {
            // Non-string type, copy data directly if buffer is sufficient.
            if (cbDataW_actual <= cbDataA_provided) {
                memcpy(lpData, dataW_vec.data(), cbDataW_actual);
                *lpcbData = cbDataW_actual;
            } else {
                *lpcbData = cbDataW_actual; // Required bytes
                return ERROR_MORE_DATA;
            }
        }
    } else if (status == ERROR_MORE_DATA) {
        // Hooked_RegQueryValueExW needs more buffer (cbDataW_actual has required WIDE bytes).
        if (typeFromW == REG_SZ || typeFromW == REG_EXPAND_SZ || typeFromW == REG_MULTI_SZ) {
             // Need to get W data to convert size to A.
            std::vector<BYTE> tempW_data(cbDataW_actual);
            DWORD temp_cbDataW = cbDataW_actual;
            LSTATUS temp_status = g_true_original_RegQueryValueExW(hKey, valueNameW.c_str(), lpReserved, &typeFromW, tempW_data.data(), &temp_cbDataW);
            if (temp_status == ERROR_SUCCESS) {
                 *lpcbData = WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWSTR>(tempW_data.data()), temp_cbDataW / sizeof(wchar_t), nullptr, 0, nullptr, nullptr);
            } else {
                *lpcbData = cbDataW_actual; // Fallback: report W bytes
            }
        } else {
            // Non-string, required W bytes is required A bytes.
            *lpcbData = cbDataW_actual;
        }
    }
    return status;
}

LSTATUS WINAPI Hooked_RegGetValueA(
    HKEY hkey,
    LPCSTR lpSubKey,
    LPCSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
) {
    if (!g_true_original_RegGetValueW) return ERROR_CALL_NOT_IMPLEMENTED;

    std::wstring subKeyW = AnsiToWide(lpSubKey);
    std::wstring valueW = AnsiToWide(lpValue);
    DWORD cbDataA_provided = 0;
    if (pcbData) cbDataA_provided = *pcbData;

    LSTATUS status;
    DWORD typeFromW = 0;

    // If pvData is NULL, querying size/type.
    if (pvData == NULL) {
        status = Hooked_RegGetValueW(hkey, subKeyW.c_str(), valueW.c_str(), dwFlags, &typeFromW, nullptr, pcbData); // pcbData gets W-bytes
        if (pdwType) *pdwType = typeFromW;

        if (status == ERROR_SUCCESS || status == ERROR_MORE_DATA) {
             if (pcbData && (typeFromW == REG_SZ || typeFromW == REG_EXPAND_SZ || typeFromW == REG_MULTI_SZ)) {
                if (*pcbData > 0) {
                    std::vector<wchar_t> tempW_data(*pcbData / sizeof(wchar_t) + 1);
                    DWORD temp_cbDataW = *pcbData;
                    LSTATUS temp_status = g_true_original_RegGetValueW(hkey, subKeyW.c_str(), valueW.c_str(), dwFlags, &typeFromW, tempW_data.data(), &temp_cbDataW);
                    if (temp_status == ERROR_SUCCESS) {
                        *pcbData = WideCharToMultiByte(CP_ACP, 0, tempW_data.data(), temp_cbDataW / sizeof(wchar_t), nullptr, 0, nullptr, nullptr);
                    } 
                }
            }
        }
        return status;
    }

    // pvData is not NULL, reading data.
    std::vector<BYTE> dataW_vec(cbDataA_provided * sizeof(wchar_t) + 2); 
    DWORD cbDataW_actual = static_cast<DWORD>(dataW_vec.size());

    status = Hooked_RegGetValueW(hkey, subKeyW.c_str(), valueW.c_str(), dwFlags, &typeFromW, dataW_vec.data(), &cbDataW_actual);
    if (pdwType) *pdwType = typeFromW;

    if (status == ERROR_SUCCESS) {
        bool isString = (typeFromW == REG_SZ || typeFromW == REG_EXPAND_SZ || typeFromW == REG_MULTI_SZ);
        // Check RRF_NOEXPAND, if present and type is REG_EXPAND_SZ, treat as REG_SZ for conversion handling (no expansion needed from our side)
        if ((dwFlags & RRF_NOEXPAND) && typeFromW == REG_EXPAND_SZ) isString = true; 

        if (isString) {
            int bytesWrittenA = WideCharToMultiByte(CP_ACP, 0, 
                                                  reinterpret_cast<LPCWSTR>(dataW_vec.data()), 
                                                  cbDataW_actual / sizeof(wchar_t), 
                                                  reinterpret_cast<LPSTR>(pvData), 
                                                  cbDataA_provided, 
                                                  nullptr, nullptr);
            if (bytesWrittenA == 0 && cbDataW_actual > 0) {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    *pcbData = WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWSTR>(dataW_vec.data()), cbDataW_actual / sizeof(wchar_t), nullptr, 0, nullptr, nullptr);
                    return ERROR_MORE_DATA;
                }
                return ERROR_CANTOPEN;
            }
            *pcbData = bytesWrittenA;
        } else { // Non-string type
            if (cbDataW_actual <= cbDataA_provided) {
                memcpy(pvData, dataW_vec.data(), cbDataW_actual);
                *pcbData = cbDataW_actual;
            } else {
                *pcbData = cbDataW_actual;
                return ERROR_MORE_DATA;
            }
        }
    } else if (status == ERROR_MORE_DATA) {
        bool isString = (typeFromW == REG_SZ || typeFromW == REG_EXPAND_SZ || typeFromW == REG_MULTI_SZ);
        if ((dwFlags & RRF_NOEXPAND) && typeFromW == REG_EXPAND_SZ) isString = true;

        if (isString) {
            std::vector<BYTE> tempW_data(cbDataW_actual);
            DWORD temp_cbDataW = cbDataW_actual;
            LSTATUS temp_status = g_true_original_RegGetValueW(hkey, subKeyW.c_str(), valueW.c_str(), dwFlags, &typeFromW, tempW_data.data(), &temp_cbDataW);
             if (pdwType) *pdwType = typeFromW; // Update type from the actual call that might get data
            if (temp_status == ERROR_SUCCESS) {
                 *pcbData = WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWSTR>(tempW_data.data()), temp_cbDataW / sizeof(wchar_t), nullptr, 0, nullptr, nullptr);
            } else {
                *pcbData = cbDataW_actual; // Fallback
            }
        } else {
            *pcbData = cbDataW_actual;
        }
    }
    return status;
}