#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// These will store the true original function pointers obtained by reg_info
// They are primarily for Detours to have a place to store the trampoline.
// Individual modules will also get these true originals via their init functions.
extern LSTATUS (WINAPI *g_true_original_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
extern LSTATUS (WINAPI *g_true_original_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);
extern LSTATUS (WINAPI *g_true_original_RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
extern LSTATUS (WINAPI *g_true_original_RegEnumKeyExW)(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME);
extern LSTATUS (WINAPI *g_true_original_RegCloseKey)(HKEY);

extern LSTATUS (WINAPI *g_true_original_RegQueryValueExA)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
extern LSTATUS (WINAPI *g_true_original_RegGetValueA)(HKEY, LPCSTR, LPCSTR, DWORD, LPDWORD, PVOID, LPDWORD);
extern LSTATUS (WINAPI *g_true_original_RegOpenKeyExA)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
extern LSTATUS (WINAPI *g_true_original_RegEnumKeyExA)(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPSTR, LPDWORD, PFILETIME);
// Note: RegCloseKey does not have A/W variants, it's a single function.

// Unified Hook Function Declarations (these will be attached by DllMain)
LSTATUS WINAPI Unified_Hooked_RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
LSTATUS WINAPI Unified_Hooked_RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime);
LSTATUS WINAPI Unified_Hooked_RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LSTATUS WINAPI Unified_Hooked_RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);
LSTATUS WINAPI Unified_Hooked_RegCloseKey(HKEY hKey);

LSTATUS WINAPI Unified_Hooked_RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
LSTATUS WINAPI Unified_Hooked_RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcchName, LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcchClass, PFILETIME lpftLastWriteTime);
LSTATUS WINAPI Unified_Hooked_RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LSTATUS WINAPI Unified_Hooked_RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData);

// Initialization and cleanup for all registry hooks
bool InitializeRegistryHooksAndHandlers(); // Renamed for clarity
void RemoveRegistryHooksAndHandlers();   // Renamed for clarity

// Functions for Detours to get the address of the function pointer variables 
// where the trampolines for the UNIFIED hooks will be stored.
// These variables will initially point to the real Windows APIs.
PVOID* GetOriginal_RegOpenKeyExW();
PVOID* GetOriginal_RegEnumKeyExW();
PVOID* GetOriginal_RegQueryValueExW();
PVOID* GetOriginal_RegGetValueW();
PVOID* GetOriginal_RegCloseKey();

PVOID* GetOriginal_RegOpenKeyExA();
PVOID* GetOriginal_RegEnumKeyExA();
PVOID* GetOriginal_RegQueryValueExA();
PVOID* GetOriginal_RegGetValueA();
