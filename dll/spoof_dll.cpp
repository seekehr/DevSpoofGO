#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h> // Should be included once per project before headers that use the GUIDs
#include <winsock2.h> // Must come before iphlpapi.h
#include <ws2tcpip.h>
#include <string>     // For std::string, std::wstring
#include <cstdio>     // For swprintf_s
#include <cstring>    // For wcslen, wcscpy_s, mbstowcs_s
#include "detours.h"  // Microsoft Detours library
#include "native/disk/vol_info.h" // For volume information related functions
#include "native/os/os_info.h"   // For computer name related functions
#include "native/hardware/hardware_info.h"      // For centralized hardware serial hooking
#include "native/reg/reg_info.h" // Centralized registry hooks 
#include "native/network/wlan_info.h" // For WLAN information related functions
#include "native/disk/disk_serial.h" // For Disk Serial spoofing and now g_real_DeviceIoControl extern declaration
#include "wmi/wmi_handler.h" // For WMI ExecQuery hooking

void LogDLL(const wchar_t* msg, HRESULT hr = S_OK) {
    if (FAILED(hr)) {
        WCHAR buf[512];
        swprintf_s(buf, L"SPOOF_DLL: %ls - HRESULT: 0x%08X", msg, hr);
        OutputDebugStringW(buf);
    } else {
        WCHAR buf[512];
        swprintf_s(buf, L"SPOOF_DLL: %ls", msg);
        OutputDebugStringW(buf);
    }
}

// --- DLL entry point ---

// Definition of the global function pointer for DeviceIoControl trampoline
BOOL(WINAPI* g_real_DeviceIoControl)(
    HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
    LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped
) = DeviceIoControl; // Initialize with the actual API function address

// static so it's shared between ATTACH and DETACH logic within DllMain
static HRESULT comInitResult = S_OK; 

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason) {
    if (DetourIsHelperProcess()) return TRUE;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        LogDLL(L"DLL_PROCESS_ATTACH - Starting initialization");
        
        DisableThreadLibraryCalls(hinstDLL);
        
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            LogDLL(L"CoInitializeEx failed", hr);
            return FALSE;
        }
        LogDLL(L"COM initialized successfully");

        if (LoadLibraryW(L"iphlpapi.dll") == NULL) {
            LogDLL(L"Failed to load iphlpapi.dll", HRESULT_FROM_WIN32(GetLastError()));
        }
        if (LoadLibraryW(L"wlanapi.dll") == NULL) {
            LogDLL(L"Failed to load wlanapi.dll", HRESULT_FROM_WIN32(GetLastError()));
        }
        if (LoadLibraryW(L"wbemcore.dll") == NULL) {
            LogDLL(L"Failed to load wbemcore.dll", HRESULT_FROM_WIN32(GetLastError()));
        }

        if (!InitializeHardwareHooks()) {
            LogDLL(L"Hardware hooks initialization failed");
            return FALSE;
        }
        LogDLL(L"Hardware hooks initialized");

        if (!InitializeRegistryHooksAndHandlers()) {
            LogDLL(L"Registry hooks initialization failed");
            return FALSE;
        }
        LogDLL(L"Registry hooks initialized");

        if (!InitializeWMIHooks()) {
            LogDLL(L"WMI hooks initialization failed");
            return FALSE;
        }
        LogDLL(L"WMI hooks initialized");

        DetourRestoreAfterWith();
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) {
            LogDLL(L"Detours transaction commit failed", error);
            return FALSE;
        }
        LogDLL(L"DLL initialization completed successfully");

    } else if (fdwReason == DLL_PROCESS_DETACH) {
        LogDLL(L"DLL_PROCESS_DETACH - Starting cleanup");
        
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        CleanupWMIHooks();
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) {
            LogDLL(L"Detours cleanup transaction failed", error);
        }
        
        CoUninitialize();
        LogDLL(L"DLL cleanup completed");
    }

    return TRUE;
}