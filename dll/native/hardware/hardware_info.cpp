#include "hardware_info.h"
#include "motherboard_serial.h"
#include "bios_serial.h"
#include "processor_id.h"
#include "system_uuid.h"
#include "../../detours/include/detours.h"
#include <windows.h>

static UINT (WINAPI *Real_GetSystemFirmwareTable_Original)(DWORD, DWORD, PVOID, DWORD) = nullptr;

UINT WINAPI Hooked_GetSystemFirmwareTable_Central(DWORD FirmwareTableProviderSignature,
                                                DWORD FirmwareTableID,
                                                PVOID pFirmwareTableBuffer,
                                                DWORD BufferSize) {
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: CRITICAL - Real_GetSystemFirmwareTable_Original is NULL in hook!");
        UINT (WINAPI *pGetSystemFirmwareTable)(DWORD, DWORD, PVOID, DWORD) =
            (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemFirmwareTable");
        if (pGetSystemFirmwareTable) {
            return pGetSystemFirmwareTable(FirmwareTableProviderSignature, FirmwareTableID, pFirmwareTableBuffer, BufferSize);
        }
        return ERROR_INVALID_FUNCTION;
    }

    UINT result = Real_GetSystemFirmwareTable_Original(FirmwareTableProviderSignature,
                                                       FirmwareTableID,
                                                       pFirmwareTableBuffer,
                                                       BufferSize);

    if (result > 0 && result <= BufferSize && 
        FirmwareTableProviderSignature == 'RSMB' && // Check for SMBIOS table
        pFirmwareTableBuffer != nullptr && BufferSize > 0) {
        ModifySmbiosForMotherboardSerial(pFirmwareTableBuffer, result);
        ModifySmbiosForBiosSerial(pFirmwareTableBuffer, result);
        ModifySmbiosForProcessorId(pFirmwareTableBuffer, result);
        ModifySmbiosForSystemUuid(pFirmwareTableBuffer, result);
    } else {
        if (result > 0 && FirmwareTableProviderSignature == 'RSMB' && pFirmwareTableBuffer != nullptr) {
            if (result > BufferSize) {
                 OutputDebugStringW(L"HARDWARE_INFO: SMBIOS modification skipped - buffer too small.");
            }
        }
    }
    return result;
}

bool InitializeHardwareHooks() {
    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        OutputDebugStringW(L"HARDWARE_INFO: CRITICAL - Failed to get handle for kernel32.dll");
        return false;
    }
    Real_GetSystemFirmwareTable_Original = (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(hKernel32, "GetSystemFirmwareTable");
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: CRITICAL - Failed to get address of GetSystemFirmwareTable.");
    }

    InitializeMotherboardSerialHooks(Real_GetSystemFirmwareTable_Original); 
    InitializeBiosSerialHooks(Real_GetSystemFirmwareTable_Original);      
    InitializeProcessorIdHooks(Real_GetSystemFirmwareTable_Original);    
    InitializeSystemUuidHooks(Real_GetSystemFirmwareTable_Original);   

    return true; 
}

void CleanupHardwareHooks() {
    if (Real_GetSystemFirmwareTable_Original != nullptr) {
        DetourDetach(
            reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Original),
            reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable_Central)
        );
        // Nullify the module's pointer, regardless of detach outcome, to indicate cleanup.
        Real_GetSystemFirmwareTable_Original = nullptr;
    }
}

PVOID* GetRealGetSystemFirmwareTableAddressStore() {
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: CRITICAL - GetRealGetSystemFirmwareTableAddressStore called when original is NULL.");
    }
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Original);
}

PVOID GetCentralHookFunctionAddress() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable_Central);
}
