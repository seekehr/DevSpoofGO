// spoof_dll.c
#include <windows.h>
#include "detours.h"  // Microsoft Detours library

// Global configuration variables
char g_computerName[256] = "SPOOFED-PC";
DWORD g_volumeSerial = 0xABCD1234;
char g_processorId[256] = "BFEBFBFF000906ED";

// Function pointers to original Windows API functions
static BOOL (WINAPI *Real_GetComputerNameA)(LPSTR, LPDWORD) = GetComputerNameA;
static BOOL (WINAPI *Real_GetVolumeInformationA)(LPCSTR, LPSTR, DWORD, LPDWORD, LPDWORD, LPDWORD, LPSTR, DWORD) = GetVolumeInformationA;

// Hook functions
BOOL WINAPI Hooked_GetComputerNameA(LPSTR lpBuffer, LPDWORD nSize) {
    if (*nSize < strlen(g_computerName) + 1) {
        *nSize = strlen(g_computerName) + 1;
        SetLastError(ERROR_BUFFER_OVERFLOW);
        return FALSE;
    }
    
    strcpy(lpBuffer, g_computerName);
    *nSize = strlen(g_computerName);
    return TRUE;
}

BOOL WINAPI Hooked_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer,
    DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
    LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) {
    
    BOOL result = Real_GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
        lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
        lpFileSystemNameBuffer, nFileSystemNameSize);
    
    if (result && lpVolumeSerialNumber != NULL) {
        *lpVolumeSerialNumber = g_volumeSerial;
    }
    
    return result;
}

// Public API for Go to configure values
__declspec(dllexport) void SetSpoofedComputerName(const char* name) {
    strncpy(g_computerName, name, sizeof(g_computerName) - 1);
}

__declspec(dllexport) void SetSpoofedVolumeSerial(DWORD serial) {
    g_volumeSerial = serial;
}

__declspec(dllexport) void SetSpoofedProcessorId(const char* id) {
    strncpy(g_processorId, id, sizeof(g_processorId) - 1);
}

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        
        // Install hooks
        DetourAttach(&(PVOID&)Real_GetComputerNameA, Hooked_GetComputerNameA);
        DetourAttach(&(PVOID&)Real_GetVolumeInformationA, Hooked_GetVolumeInformationA);
        
        DetourTransactionCommit();
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        
        // Remove hooks
        DetourDetach(&(PVOID&)Real_GetComputerNameA, Hooked_GetComputerNameA);
        DetourDetach(&(PVOID&)Real_GetVolumeInformationA, Hooked_GetVolumeInformationA);
        
        DetourTransactionCommit();
    }
    
    return TRUE;
}