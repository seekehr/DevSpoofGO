#include "hardware_info.h"
#include "motherboard_serial.h"
#include "bios_serial.h"
#include "processor_id.h"
#include "system_uuid.h"
#include <windows.h>
#include <cstdio>

static UINT (WINAPI *Real_GetSystemFirmwareTable_Original)(DWORD, DWORD, PVOID, DWORD) = nullptr;
// Removed static pointers for Real_RegQueryValueExW and Real_RegGetValueW

UINT WINAPI Hooked_GetSystemFirmwareTable_Central(DWORD FirmwareTableProviderSignature,
                                                DWORD FirmwareTableID,
                                                PVOID pFirmwareTableBuffer,
                                                DWORD BufferSize) {
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: Real_GetSystemFirmwareTable_Original is NULL in hook!");
         // Attempt to get it directly if null, though it should be set by InitializeHardwareHooks
         UINT (WINAPI *pGetSystemFirmwareTable)(DWORD, DWORD, PVOID, DWORD) =
            (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemFirmwareTable");
        if (pGetSystemFirmwareTable) {
            return pGetSystemFirmwareTable(FirmwareTableProviderSignature, FirmwareTableID, pFirmwareTableBuffer, BufferSize);
        }
        return ERROR_INVALID_FUNCTION; // Or some other error code indicating failure
    }

    UINT result = Real_GetSystemFirmwareTable_Original(FirmwareTableProviderSignature,
                                                       FirmwareTableID,
                                                       pFirmwareTableBuffer,
                                                       BufferSize);

    if (result > 0 && result <= BufferSize && 
        FirmwareTableProviderSignature == 'RSMB' && // Check for SMBIOS table
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
            // Potentially log other reasons for not modifying if conditions aren't met
        }
    }
    OutputDebugStringW(L"HARDWARE_INFO: Hooked_GetSystemFirmwareTable_Central called.");
    return result;
}

bool InitializeHardwareHooks() {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        OutputDebugStringW(L"HARDWARE_INFO: Failed to get handle for kernel32.dll");
        return false;
    }
    Real_GetSystemFirmwareTable_Original = (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(hKernel32, "GetSystemFirmwareTable");
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: Failed to get address of GetSystemFirmwareTable.");
        // Continue, as other hardware hooks might be independent or other parts of the DLL might function.
    }

    InitializeMotherboardSerialHooks(Real_GetSystemFirmwareTable_Original); 
    InitializeBiosSerialHooks(Real_GetSystemFirmwareTable_Original);      
    InitializeProcessorIdHooks(Real_GetSystemFirmwareTable_Original);    
    InitializeSystemUuidHooks(Real_GetSystemFirmwareTable_Original);   

    return true; // Return true if kernel32 was found, specific hook failures logged internally
}

void RemoveHardwareHooks() {
    OutputDebugStringW(L"HARDWARE_INFO: Removing hardware hooks.");
}

PVOID* GetRealGetSystemFirmwareTableAddressStore() {
    if (!Real_GetSystemFirmwareTable_Original) {
        // This case should ideally not be hit if InitializeHardwareHooks was called and succeeded at least partially.
        OutputDebugStringW(L"HARDWARE_INFO: Warning - GetRealGetSystemFirmwareTableAddressStore called when original is NULL.");
    }
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Original);
}

PVOID GetCentralHookFunctionAddress() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable_Central);
}
