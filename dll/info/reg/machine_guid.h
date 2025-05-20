#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// Typedefs for original function pointers
typedef LSTATUS (WINAPI *PFN_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LSTATUS (WINAPI *PFN_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);

extern "C" {
    // Sets the MachineGuid value to be spoofed.
    __declspec(dllexport) void SetSpoofedMachineGuid(const char* guidString);
}

// Handler functions for MachineGuid-related registry operations
void Handle_RegQueryValueExW_ForMachineGuid(
    HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, 
    LPBYTE lpData, LPDWORD lpcbData,
    LSTATUS& status, bool& handled
);

void Handle_RegGetValueW_ForMachineGuid(
    HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, 
    LPDWORD pdwType, PVOID pvData, LPDWORD pcbData,
    LSTATUS& status, bool& handled
);

// Initializes MachineGuid spoofing data and stores true original function pointers.
void InitializeMachineGuid_Centralized(
    PVOID pTrueOriginalRegQueryValueExW, 
    PVOID pTrueOriginalRegGetValueW
);
