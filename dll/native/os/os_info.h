#pragma once
#include <windows.h>

// --- Function declarations ---

// Hook functions
BOOL WINAPI Hooked_GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize);
BOOL WINAPI Hooked_GetComputerNameW(LPWSTR lpBuffer, LPDWORD nSize);

// API for the injector
extern "C" {
    __declspec(dllexport) void SetSpoofedComputerName(const char* name);
}

// Helper functions for hooking from spoof_dll.cpp
PVOID* GetRealGetComputerNameA();
PVOID* GetRealGetComputerNameW();
PVOID GetHookedGetComputerNameA();
PVOID GetHookedGetComputerNameW();

// Access to the spoofed computer name (for other modules if needed)
const WCHAR* GetSpoofedComputerName(); 