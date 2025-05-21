#pragma once
#include <windows.h>

// Function pointers to original Windows API functions
extern BOOL (WINAPI *Real_GetComputerNameW)(LPWSTR lpBuffer, LPDWORD nSize);
extern BOOL (WINAPI *Real_GetComputerNameExW)(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize);

// Hooked function prototypes
BOOL WINAPI Hooked_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);
BOOL WINAPI Hooked_GetComputerNameExW(COMPUTER_NAME_FORMAT NameType, LPWSTR lpBuffer, LPDWORD nSize);

// Exported function to set the spoofed computer name (optional)
extern "C" {
    __declspec(dllexport) void SetSpoofedComputerName(const wchar_t* name);
}

// Helper functions for hooking from spoof_dll.cpp
PVOID* GetRealGetComputerNameW();
PVOID GetHookedGetComputerNameW();

// Access to the spoofed computer name (for other modules if needed)
const WCHAR* GetSpoofedComputerName();

// Initialization and cleanup functions
bool InitializeOSInfoHooks();
void CleanupOSInfoHooks(); 