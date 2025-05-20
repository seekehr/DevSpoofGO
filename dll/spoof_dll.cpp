#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h> // Must come before iphlpapi.h
#include <ws2tcpip.h>
#include <string>     // For std::string, std::wstring
#include <cstdio>     // For swprintf_s
#include <cstring>    // For wcslen, wcscpy_s, mbstowcs_s
#include "detours.h"  // Microsoft Detours library
#include "info/disk/vol_info.h" // For volume information related functions
#include "info/os/os_info.h"   // For computer name related functions
#include "info/hardware/hardware_info.h"      // For centralized hardware serial hooking
#include "info/reg/reg_info.h" // Centralized registry hooks 
#include "info/network/wlan_info.h" // For WLAN information related functions
#include "info/disk/disk_serial.h" // For Disk Serial spoofing and now g_real_DeviceIoControl extern declaration

// --- DLL entry point ---

// Definition of the global function pointer for DeviceIoControl trampoline
BOOL(WINAPI* g_real_DeviceIoControl)(
    HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize,
    LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped
) = DeviceIoControl; // Initialize with the actual API function address

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason) {
    // DetourIsHelperProcess is used by Detours internally, return TRUE if it's a helper
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);

        LoadLibraryW(L"iphlpapi.dll");
        LoadLibraryW(L"wlanapi.dll");

        if (!InitializeHardwareHooks()) {
            OutputDebugStringW(L"spoof_dll.dll: Failed to initialize hardware hooks.");
        }

        // Initialize Registry Hooks (MachineGuid, Certificates)
        if (!InitializeRegistryHooksAndHandlers()) { // Centralized registry hook and handler initialization
            OutputDebugStringW(L"spoof_dll.dll: Failed to initialize registry hooks/handlers.");
        } else {
            OutputDebugStringW(L"spoof_dll.dll: Registry hooks/handlers initialized.");
        }

        DetourRestoreAfterWith();

        // Begin Detours transaction to apply hooks atomically
        DetourTransactionBegin();
        // Update the current thread within the transaction
        DetourUpdateThread(GetCurrentThread());

        // --- Install hooks ---
        // Computer name hooks
        DetourAttach(GetRealGetComputerNameW(), GetHookedGetComputerNameW());
        DetourAttach(GetRealGetComputerNameA(), GetHookedGetComputerNameA());
        
        // Volume information hooks
        DetourAttach(GetRealGetVolumeInformationA(), GetHookedGetVolumeInformationA());
        DetourAttach(GetRealGetVolumeInformationW(), GetHookedGetVolumeInformationW());
        
        // Hardware information hooks (using the centralized hook)
        DetourAttach(GetRealGetSystemFirmwareTableAddressStore(), GetCentralHookFunctionAddress());
        
        // Disk Serial hook for DeviceIoControl
        DetourAttach(&(PVOID&)g_real_DeviceIoControl, HookedDeviceIoControlForDiskSerial);

        DetourAttach(GetOriginal_RegOpenKeyExW(), Unified_Hooked_RegOpenKeyExW);
        DetourAttach(GetOriginal_RegEnumKeyExW(), Unified_Hooked_RegEnumKeyExW);
        DetourAttach(GetOriginal_RegQueryValueExW(), Unified_Hooked_RegQueryValueExW);
        DetourAttach(GetOriginal_RegGetValueW(), Unified_Hooked_RegGetValueW);
        DetourAttach(GetOriginal_RegCloseKey(), Unified_Hooked_RegCloseKey);

        DetourAttach(GetRealGetAdaptersInfo(), GetHookedGetAdaptersInfo());
        DetourAttach(GetRealGetAdaptersAddresses(), GetHookedGetAdaptersAddresses());
        DetourAttach(GetRealWlanEnumInterfaces(), GetHookedWlanEnumInterfaces());
        DetourAttach(GetRealWlanQueryInterface(), GetHookedWlanQueryInterface());
        DetourAttach(GetRealWlanGetNetworkBssList(), GetHookedWlanGetNetworkBssList());

        LONG error = DetourTransactionCommit();

        // --- Check for errors and log ---
        if (error != NO_ERROR) {
            WCHAR debugMsg[256];
            swprintf_s(debugMsg, L"spoof_dll.dll: Detours attach failed with error: %d (OS Error: %d)\n", error, GetLastError());
            OutputDebugStringW(debugMsg);
        } else {
            OutputDebugStringW(L"spoof_dll.dll: Detours attached successfully!\n");
        }

    } else if (fdwReason == DLL_PROCESS_DETACH) {
        // When the DLL is detached (process exits or DLL is unloaded)

        // Begin Detours transaction to remove hooks atomically
        DetourTransactionBegin();
        // Update the current thread within the transaction
        DetourUpdateThread(GetCurrentThread());

        // --- Remove hooks ---
        // Computer name hooks
        DetourDetach(GetRealGetComputerNameW(), GetHookedGetComputerNameW());
        DetourDetach(GetRealGetComputerNameA(), GetHookedGetComputerNameA());
        
        // Volume information hooks
        DetourDetach(GetRealGetVolumeInformationA(), GetHookedGetVolumeInformationA());
        DetourDetach(GetRealGetVolumeInformationW(), GetHookedGetVolumeInformationW());
        
        // Hardware information hooks (using the centralized hook)
        DetourDetach(GetRealGetSystemFirmwareTableAddressStore(), GetCentralHookFunctionAddress());
        
        // Disk Serial hook for DeviceIoControl
        DetourDetach(&(PVOID&)g_real_DeviceIoControl, HookedDeviceIoControlForDiskSerial);

        // Unified Registry Hooks (using new GetOriginal_ prefix)
        DetourDetach(GetOriginal_RegOpenKeyExW(), Unified_Hooked_RegOpenKeyExW);
        DetourDetach(GetOriginal_RegEnumKeyExW(), Unified_Hooked_RegEnumKeyExW);
        DetourDetach(GetOriginal_RegQueryValueExW(), Unified_Hooked_RegQueryValueExW);
        DetourDetach(GetOriginal_RegGetValueW(), Unified_Hooked_RegGetValueW);
        DetourDetach(GetOriginal_RegCloseKey(), Unified_Hooked_RegCloseKey);

        // Network information hooks
        DetourDetach(GetRealGetAdaptersInfo(), GetHookedGetAdaptersInfo());
        DetourDetach(GetRealGetAdaptersAddresses(), GetHookedGetAdaptersAddresses());
        DetourDetach(GetRealWlanEnumInterfaces(), GetHookedWlanEnumInterfaces());
        DetourDetach(GetRealWlanQueryInterface(), GetHookedWlanQueryInterface());
        DetourDetach(GetRealWlanGetNetworkBssList(), GetHookedWlanGetNetworkBssList());

        // Commit the transaction - this removes the hooks
        DetourTransactionCommit();

        RemoveHardwareHooks();
        RemoveRegistryHooksAndHandlers(); // Cleanup registry hooks/handlers states

        OutputDebugStringW(L"spoof_dll.dll: Detours detached.\n");
    }

    return TRUE; 
}