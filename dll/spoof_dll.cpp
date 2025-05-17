#include <windows.h>
#include <string>     // For std::string, std::wstring
#include <cstdio>     // For swprintf_s
#include <cstring>    // For wcslen, wcscpy_s, mbstowcs_s
#include "detours.h"  // Microsoft Detours library
#include "info/disk_info.h" // For volume information related functions
#include "info/os_info.h"   // For computer name related functions
#include "info/hardware_info.h" // For hardware information related functions

// --- DLL entry point ---
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason) {
    // DetourIsHelperProcess is used by Detours internally, return TRUE if it's a helper
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (fdwReason == DLL_PROCESS_ATTACH) {
        // Disable thread library calls to improve performance for simple DLLs
        DisableThreadLibraryCalls(hinstDLL);

        // DetourRestoreAfterWith is needed if the target process is instrumented by Detours before
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
        
        // Hardware information hooks
        DetourAttach(GetRealGetSystemFirmwareTable(), GetHookedGetSystemFirmwareTable());

        // Commit the transaction - this applies the hooks
        LONG error = DetourTransactionCommit();

        // --- Check for errors and log ---
        if (error != NO_ERROR) {
            // Hook attachment failed! Log the error code.
            WCHAR debugMsg[256];
            swprintf_s(debugMsg, L"spoof_dll.dll: Detours attach failed with error: %d (Error code %d)\n", error, GetLastError());
            OutputDebugStringW(debugMsg);
            // In a real scenario, you might want to handle this more robustly,
            // potentially preventing the DLL from fully initializing or informing the user.
        } else {
            // Hook attachment succeeded
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
        
        // Hardware information hooks
        DetourDetach(GetRealGetSystemFirmwareTable(), GetHookedGetSystemFirmwareTable());

        // Commit the transaction - this removes the hooks
        DetourTransactionCommit();

        OutputDebugStringW(L"spoof_dll.dll: Detours detached.\n");
    }

    return TRUE; // Return TRUE to indicate success
}