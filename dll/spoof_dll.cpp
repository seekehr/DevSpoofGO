#include <windows.h>
#include <string>     // For std::string, std::wstring
#include <cstdio>     // For swprintf_s
#include <cstring>    // For wcslen, wcscpy_s, mbstowcs_s
#include "detours.h"  // Microsoft Detours library

// --- Global configuration variables ---
// Use WCHAR for the spoofed computer name as we are hooking GetComputerNameW
WCHAR g_computerNameW[256] = L"SPOOFED-PC-DEFAULT"; // Default value
DWORD g_volumeSerial = 0xABCD1234;                 // Spoofed volume serial
char g_processorId[256] = "BFEBFBFF000906ED";      // Spoofed processor ID (Note: Spoofing this requires hooking other APIs like GetCpuid/CPUID instruction)

// --- Function pointers to original Windows API functions ---
// Pointer to the original GetComputerNameW function
static BOOL (WINAPI *Real_GetComputerNameW)(LPWSTR, LPDWORD) = GetComputerNameW;

// Pointer to the original GetVolumeInformationA function (keep if targeting ANSI callers)
static BOOL (WINAPI *Real_GetVolumeInformationA)(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPSTR, DWORD) = GetVolumeInformationA;

// Pointer to the original GetVolumeInformationW function (add if targeting Unicode callers or both)
static BOOL (WINAPI *Real_GetVolumeInformationW)(LPCWSTR, LPWSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPWSTR, DWORD) = GetVolumeInformationW;

// --- Hook functions ---

// Hook function for GetComputerNameW - takes and returns wide characters (LPWSTR)
BOOL WINAPI Hooked_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize) {
    // Calculate the length of the spoofed name (excluding null terminator)
    size_t spoofedNameLen = wcslen(g_computerNameW);
    // Calculate the required buffer size (including null terminator)
    size_t requiredSize = spoofedNameLen + 1;

    // Check if the provided buffer is large enough
    if (*nSize < requiredSize) {
        *nSize = static_cast<DWORD>(requiredSize); // Set the required size
        SetLastError(ERROR_BUFFER_OVERFLOW); // Indicate buffer overflow
        return FALSE; // Return FALSE to signal failure
    }

    // Copy the spoofed name to the output buffer
    wcscpy_s(lpBuffer, *nSize, g_computerNameW); // Use secure wide-character copy

    // Set the size returned to the caller (length of the string, excluding null terminator)
    *nSize = static_cast<DWORD>(spoofedNameLen);

    // Return TRUE to signal success
    return TRUE;
}

// Hook function for GetVolumeInformationA (keep if needed)
BOOL WINAPI Hooked_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {

    // Call the original function first
    BOOL result = Real_GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
        lpFileSystemNameBuffer, nFileSystemNameSize);

    // If the original call succeeded and the caller provided a buffer for the serial number
    if (result && lpVolumeSerialNumber != nullptr) {
        // Overwrite the serial number with our spoofed value
        *lpVolumeSerialNumber = g_volumeSerial;
    }

    // Return the result of the original call
    return result;
}

BOOL WINAPI Hooked_GetVolumeInformationW(LPCWSTR lpRootPathName, LPWSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPWSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {

    BOOL result = Real_GetVolumeInformationW(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
        lpFileSystemNameBuffer, nFileSystemNameSize);

    if (result && lpVolumeSerialNumber != NULL) {
        *lpVolumeSerialNumber = g_volumeSerial;
    }

    return result;
}


// --- Public API for Injector (Go program) to configure values ---
// Ensure these are exported with C linkage (no name mangling)
extern "C" {
    // Function to set the spoofed computer name (input is ANSI string)
    __declspec(dllexport) void SetSpoofedComputerName(const char* name) {
        if (name) {
            // Convert the input ANSI string (char*) to a wide character string (WCHAR*)
            const size_t len = strlen(name) + 1; // Include space for null terminator
            size_t converted = 0;
            // Use mbstowcs_s for secure conversion to WCHAR
            mbstowcs_s(&converted, g_computerNameW, sizeof(g_computerNameW) / sizeof(WCHAR), name, len - 1);
            // mbstowcs_s automatically null-terminates if the source fits.
            // If using mbstowcs, ensure manual null termination: g_computerNameW[sizeof(g_computerNameW) / sizeof(WCHAR) - 1] = L'\0';
        }
    }

    // Function to set the spoofed volume serial number
    __declspec(dllexport) void SetSpoofedVolumeSerial(DWORD serial) {
        g_volumeSerial = serial;
    }

    // Function to set the spoofed processor ID
    __declspec(dllexport) void SetSpoofedProcessorId(const char* id) {
        if (id) {
            // Note: Spoofing processor ID is complex and requires hooking different APIs
            // or methods than just GetComputerName. This function only sets a global variable.
            strncpy_s(g_processorId, sizeof(g_processorId), id, _TRUNCATE);
        }
    }
}

// --- DLL entry point ---
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason) {
    // DetourIsHelperProcess is used by Detours internally, return TRUE if it's a helper
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (fdwReason == DLL_PROCESS_ATTACH) {
        // Disable thread library calls to improve performance for simple DLLs
        DisableThreadLibraryCalls(hinstDLL);

        // DetourRestoreAfterWith is needed if the target process is instrumented by Detours before
        DetourRestoreAfterWith();

        // Begin Detours transaction to apply hooks atomically
        DetourTransactionBegin();
        // Update the current thread within the transaction
        DetourUpdateThread(GetCurrentThread());

        // --- Install hooks ---
        // Attach our hook function (Hooked_GetComputerNameW) to the real function (Real_GetComputerNameW points to GetComputerNameW)
        // The &(PVOID&) cast is required by Detours to cast a typed function pointer to a PVOID&
        DetourAttach(&reinterpret_cast<PVOID &>(Real_GetComputerNameW), Hooked_GetComputerNameW);
        DetourAttach(&reinterpret_cast<PVOID &>(Real_GetVolumeInformationA), Hooked_GetVolumeInformationA);
        DetourAttach(&reinterpret_cast<PVOID &>(Real_GetVolumeInformationW), Hooked_GetVolumeInformationW);


        // Commit the transaction - this applies the hooks
        LONG error = DetourTransactionCommit();

        // --- Check for errors and log ---
        if (error != NO_ERROR) {
            // Hook attachment failed! Log the error code.
            WCHAR debugMsg[256];
            swprintf_s(debugMsg, L"spoof_dll.dll: Detours attach failed with error: %d (Error code %d)\n", error, GetLastError());
            OutputDebugStringW(debugMsg);
            // In a real scenario, you might want to handle this more robustly,
            // potentially preventing the DLL from fully initializing or informing the user.
        } else {
            // Hook attachment succeeded
            OutputDebugStringW(L"spoof_dll.dll: Detours attached successfully!\n");
        }

    } else if (fdwReason == DLL_PROCESS_DETACH) {
        // When the DLL is detached (process exits or DLL is unloaded)

        // Begin Detours transaction to remove hooks atomically
        DetourTransactionBegin();
        // Update the current thread within the transaction
        DetourUpdateThread(GetCurrentThread());

        // --- Remove hooks ---
        // Detach our hook function from the real function
        DetourDetach(&reinterpret_cast<PVOID &>(Real_GetComputerNameW), Hooked_GetComputerNameW);
        DetourDetach(&reinterpret_cast<PVOID &>(Real_GetVolumeInformationA), Hooked_GetVolumeInformationA);
        DetourDetach(&reinterpret_cast<PVOID &>(Real_GetVolumeInformationW), Hooked_GetVolumeInformationW);

        // Commit the transaction - this removes the hooks
        DetourTransactionCommit();

        OutputDebugStringW(L"spoof_dll.dll: Detours detached.\n");
    }

    return TRUE; // Return TRUE to indicate success
}