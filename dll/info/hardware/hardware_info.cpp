#include "hardware_info.h"
#include "motherboard_serial.h"
#include "bios_serial.h"
#include "processor_id.h"
#include "system_uuid.h"
#include "machine_guid.h"
#include <windows.h>
#include <cstdio>

static UINT (WINAPI *Real_GetSystemFirmwareTable_Original)(DWORD, DWORD, PVOID, DWORD) = nullptr;
// Static pointers for original registry functions, similar to Real_GetSystemFirmwareTable_Original
static LSTATUS (WINAPI *Real_RegQueryValueExW)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = nullptr;
static LSTATUS (WINAPI *Real_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD) = nullptr;

UINT WINAPI Hooked_GetSystemFirmwareTable_Central(DWORD FirmwareTableProviderSignature,
                                                DWORD FirmwareTableID,
                                                PVOID pFirmwareTableBuffer,
                                                DWORD BufferSize) {
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: Real_GetSystemFirmwareTable_Original is NULL in hook!");
         UINT (WINAPI *pGetSystemFirmwareTable)(DWORD, DWORD, PVOID, DWORD) =
            (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemFirmwareTable");
        if (pGetSystemFirmwareTable) {
            return pGetSystemFirmwareTable(FirmwareTableProviderSignature, FirmwareTableID, pFirmwareTableBuffer, BufferSize);
        }
        return 0;
    }

    UINT result = Real_GetSystemFirmwareTable_Original(FirmwareTableProviderSignature,
                                                       FirmwareTableID,
                                                       pFirmwareTableBuffer,
                                                       BufferSize);

    if (result > 0 && result <= BufferSize && 
        FirmwareTableProviderSignature == 'RSMB' &&
        pFirmwareTableBuffer != nullptr && BufferSize > 0) {
        OutputDebugStringW(L"HARDWARE_INFO: Modifying SMBIOS data.");
        ModifySmbiosForMotherboardSerial(pFirmwareTableBuffer, result);
        ModifySmbiosForBiosSerial(pFirmwareTableBuffer, result);
        ModifySmbiosForProcessorId(pFirmwareTableBuffer, result);
        ModifySmbiosForSystemUuid(pFirmwareTableBuffer, result);
    } else {
        if (result > 0 && FirmwareTableProviderSignature == 'RSMB' && pFirmwareTableBuffer != nullptr) {
            if (result > BufferSize) {
                 WCHAR debugMsg[256];
                 swprintf_s(debugMsg, L"HARDWARE_INFO: SMBIOS modification skipped - buffer too small. Required: %u, Provided: %u", result, BufferSize);
                 OutputDebugStringW(debugMsg);
            }
        }
    }
    return result;
}

bool InitializeHardwareHooks() {
    OutputDebugStringW(L"HARDWARE_INFO: Initializing hardware hooks...");
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        OutputDebugStringW(L"HARDWARE_INFO: Failed to get handle for kernel32.dll");
        return false;
    }
    Real_GetSystemFirmwareTable_Original = (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(hKernel32, "GetSystemFirmwareTable");
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: Failed to get address of GetSystemFirmwareTable.");
        // No return false here, as other hooks might still work or be independent
    }

    HMODULE hAdvapi32 = GetModuleHandleW(L"advapi32.dll");
    if (!hAdvapi32) {
        hAdvapi32 = LoadLibraryW(L"advapi32.dll"); // Attempt to load if not already loaded
        if (!hAdvapi32) {
            OutputDebugStringW(L"HARDWARE_INFO: Failed to get handle for advapi32.dll for MachineGuid hooks.");
            // Proceeding without MachineGuid hooks if advapi32 cannot be loaded.
        } else {
            OutputDebugStringW(L"HARDWARE_INFO: Loaded advapi32.dll for MachineGuid hooks.");
        }
    }
    
    if (hAdvapi32) {
        Real_RegQueryValueExW = (LSTATUS (WINAPI *)(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD))GetProcAddress(hAdvapi32, "RegQueryValueExW");
        if (!Real_RegQueryValueExW) {
            OutputDebugStringW(L"HARDWARE_INFO: Failed to get address of RegQueryValueExW.");
        }
        Real_RegGetValueW = (LSTATUS (WINAPI *)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD))GetProcAddress(hAdvapi32, "RegGetValueW");
        if (!Real_RegGetValueW) {
            OutputDebugStringW(L"HARDWARE_INFO: Failed to get address of RegGetValueW.");
        }
    } else {
        // Log that MachineGuid hooks will not be initialized due to missing advapi32
        OutputDebugStringW(L"HARDWARE_INFO: advapi32.dll not available. MachineGuid hooks will not be initialized.");
    }

    InitializeMotherboardSerialHooks(Real_GetSystemFirmwareTable_Original);
    InitializeBiosSerialHooks(Real_GetSystemFirmwareTable_Original);
    InitializeProcessorIdHooks(Real_GetSystemFirmwareTable_Original);
    InitializeSystemUuidHooks(Real_GetSystemFirmwareTable_Original);
    // Pass the potentially null pointers. InitializeMachineGuidHooks should handle them gracefully.
    InitializeMachineGuidHooks(reinterpret_cast<PVOID>(Real_RegQueryValueExW), reinterpret_cast<PVOID>(Real_RegGetValueW)); 
    return true;
}

void RemoveHardwareHooks() {
    Real_GetSystemFirmwareTable_Original = nullptr;
    // Also nullify the registry function pointers if they were set
    Real_RegQueryValueExW = nullptr;
    Real_RegGetValueW = nullptr;
}

PVOID* GetRealGetSystemFirmwareTableAddressStore() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Original);
}

PVOID GetCentralHookFunctionAddress() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable_Central);
}
