#include <windows.h>
#include <cstring>    // For strlen, wcscpy_s, mbstowcs_s
#include "os_info.h"

// --- OS information related global variables ---
// Use WCHAR for the spoofed computer name as we are hooking GetComputerNameW
WCHAR g_computerNameW[256] = L"SPOOFED-PC-DEFAULT"; // Default value

// --- Function pointers to original Windows API functions ---
// Pointer to the original GetComputerNameA function
static BOOL (WINAPI *Real_GetComputerNameA)(LPSTR, LPDWORD) = GetComputerNameA;
// Pointer to the original GetComputerNameW function
static BOOL (WINAPI *Real_GetComputerNameW)(LPWSTR, LPDWORD) = GetComputerNameW;

// --- Hook functions ---

// Hook function for GetComputerNameA - takes and returns ANSI characters
BOOL WINAPI Hooked_GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize) {
    // First, we need to convert our Unicode computer name to ANSI
    char ansiName[256];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, ansiName, sizeof(ansiName), g_computerNameW, _TRUNCATE);

    // Calculate the length of the spoofed name (excluding null terminator)
    size_t spoofedNameLen = strlen(ansiName);
    // Calculate the required buffer size (including null terminator)
    size_t requiredSize = spoofedNameLen + 1;

    // Check if the provided buffer is large enough
    if (*nSize < requiredSize) {
        *nSize = static_cast<DWORD>(requiredSize); // Set the required size
        SetLastError(ERROR_BUFFER_OVERFLOW); // Indicate buffer overflow
        return FALSE; // Return FALSE to signal failure
    }

    // Copy the spoofed name to the output buffer
    strcpy_s(lpBuffer, *nSize, ansiName); // Use secure character copy

    // Set the size returned to the caller (length of the string, excluding null terminator)
    *nSize = static_cast<DWORD>(spoofedNameLen);

    // Return TRUE to signal success
    return TRUE;
}

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

// Function to set the spoofed computer name
void SetSpoofedComputerName(const char* name) {
    if (name) {
        // Convert the input ANSI string (char*) to a wide character string (WCHAR*)
        const size_t len = strlen(name) + 1; // Include space for null terminator
        size_t converted = 0;
        // Use mbstowcs_s for secure conversion to WCHAR
        mbstowcs_s(&converted, g_computerNameW, sizeof(g_computerNameW) / sizeof(WCHAR), name, len - 1);
        // mbstowcs_s automatically null-terminates if the source fits.
    }
}

// Function to get pointers to the original functions for hooking
PVOID* GetRealGetComputerNameA() {
    return reinterpret_cast<PVOID*>(&Real_GetComputerNameA);
}

PVOID* GetRealGetComputerNameW() {
    return reinterpret_cast<PVOID*>(&Real_GetComputerNameW);
}

// Function to get pointers to the hook functions
PVOID GetHookedGetComputerNameA() {
    return reinterpret_cast<PVOID>(Hooked_GetComputerNameA);
}

PVOID GetHookedGetComputerNameW() {
    return reinterpret_cast<PVOID>(Hooked_GetComputerNameW);
}

// Function to get the spoofed computer name for other modules
const WCHAR* GetSpoofedComputerName() {
    return g_computerNameW;
}
