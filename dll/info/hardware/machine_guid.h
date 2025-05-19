#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

extern "C" {
    __declspec(dllexport) void SetSpoofedMachineGuid(const char* guidString);
}

// Initialization function, to be called from hardware_info.cpp
void InitializeMachineGuidHooks(PVOID pOriginalRegQueryValueExW, PVOID pOriginalRegGetValueW);

LSTATUS WINAPI Hooked_RegQueryValueExW(
    HKEY hKey,
    LPCWSTR lpValueName,
    LPDWORD lpReserved,
    LPDWORD lpType,
    LPBYTE lpData,
    LPDWORD lpcbData
);

LSTATUS WINAPI Hooked_RegGetValueW(
    HKEY hkey,
    LPCWSTR lpSubKey,
    LPCWSTR lpValue,
    DWORD dwFlags,
    LPDWORD pdwType,
    PVOID pvData,
    LPDWORD pcbData
);

PVOID* GetOriginalRegQueryValueExWPtr();
PVOID* GetOriginalRegGetValueWPtr();
