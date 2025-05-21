#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h> 
#include <winsock2.h> 
#include <ws2tcpip.h>
#include <cstdio>     
#include <cstring>   
#include "detours/include/detours.h" 
#include "native/disk/vol_info.h"
#include "native/os/os_info.h"  
#include "native/hardware/hardware_info.h"      
#include "native/reg/reg_info.h"
#include "native/network/wlan_info.h" 
#include "native/disk/disk_serial.h" 
#include "wmi/wmi_handler.h" 

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


// Definition of the global function pointer for DeviceIoControl trampoline
BOOL(WINAPI* g_real_DeviceIoControl)(
    HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
    LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped
) = DeviceIoControl; 

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason) {
    if (DetourIsHelperProcess()) return TRUE;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        
        DisableThreadLibraryCalls(hinstDLL);
        
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            LogDLL(L"CoInitializeEx failed", hr);
            return FALSE;
        }

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

        if (!InitializeRegistryHooksAndHandlers()) {
            LogDLL(L"Registry hooks initialization failed");
            return FALSE;
        }

        if (!InitializeNetworkHooks()) {
            LogDLL(L"Network hooks initialization failed");
            return FALSE;
        };

        if (!InitializeWMIHooks()) {
            LogDLL(L"WMI hooks initialization failed");
            return FALSE;
        }

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        if (!InitializeDiskSerialHooks()) {
            LogDLL(L"Disk Serial hooks initialization failed");
            return FALSE;
        }

        if (!InitializeVolumeInfoHooks()) {
            LogDLL(L"Volume Info hooks initialization failed");
            return FALSE;
        }

        if (!InitializeOSInfoHooks()) {
            LogDLL(L"OS Info hooks initialization failed");
            return FALSE;
        }

        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) {
            LogDLL(L"Detours transaction commit failed", error);
            return FALSE;
        }
        LogDLL(L"DLL initialization completed successfully");

    } else if (fdwReason == DLL_PROCESS_DETACH) {
        
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        
        CleanupNetworkHooks(); 
        CleanupDiskSerialHooks(); 
        CleanupVolumeInfoHooks();
        CleanupOSInfoHooks();
        CleanupRegistryHooksAndHandlers();
        CleanupHardwareHooks(); 
        CleanupWMIHooks();
        
        CoUninitialize();
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) {
            LogDLL(L"Detours cleanup transaction failed", error);
        }
        
        LogDLL(L"DLL cleanup completed");
    }

    return TRUE;
}