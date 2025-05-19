#include "hardware_info.h"
#include "motherboard_serial.h"
#include "bios_serial.h"
#include "processor_id.h"
#include "system_uuid.h"
#include <windows.h>
#include <cstdio>

// --- Static pointer for the original GetSystemFirmwareTable function ---
static UINT (WINAPI *Real_GetSystemFirmwareTable_Original)(DWORD, DWORD, PVOID, DWORD) = nullptr;

// --- Central Hooked Function ---
UINT WINAPI Hooked_GetSystemFirmwareTable_Central(DWORD FirmwareTableProviderSignature,
                                                DWORD FirmwareTableID,
                                                PVOID pFirmwareTableBuffer,
                                                DWORD BufferSize) {
    if (!Real_GetSystemFirmwareTable_Original) {
        OutputDebugStringW(L"HARDWARE_INFO: Real_GetSystemFirmwareTable_Original is NULL in hook!");
        // Attempt to call the genuine function directly if the pointer is somehow null
         UINT (WINAPI *pGetSystemFirmwareTable)(DWORD, DWORD, PVOID, DWORD) =
            (UINT (WINAPI *)(DWORD, DWORD, PVOID, DWORD))GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemFirmwareTable");
        if (pGetSystemFirmwareTable) {
            return pGetSystemFirmwareTable(FirmwareTableProviderSignature, FirmwareTableID, pFirmwareTableBuffer, BufferSize);
        }
        return 0; // Indicate failure
    }

    // Call the original function to get the actual data
    UINT result = Real_GetSystemFirmwareTable_Original(FirmwareTableProviderSignature,
                                                       FirmwareTableID,
                                                       pFirmwareTableBuffer,
                                                       BufferSize);

    // Only modify the data if:
    // 1. The call was successful (result > 0 indicates bytes returned or required buffer size)
    // 2. We're looking at SMBIOS data ('RSMB')
    // 3. We have a buffer to work with (pFirmwareTableBuffer is not NULL)
    // 4. The BufferSize indicates that pFirmwareTableBuffer is not just for size query (BufferSize > 0)
    //    and the returned 'result' matches 'BufferSize' or is less (meaning data was written).
    //    If 'result' > 'BufferSize', it means the buffer was too small, and no data was written.
    if (result > 0 && result <= BufferSize && 
        FirmwareTableProviderSignature == 'RSMB' &&
        pFirmwareTableBuffer != nullptr && BufferSize > 0) {

        OutputDebugStringW(L"HARDWARE_INFO: Modifying SMBIOS data.");

        // Call motherboard serial modification function
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

// --- Initialization and Helper Functions ---
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
        return false;
    }

    // Initialize individual modules, passing them the original function pointer
    InitializeMotherboardSerialHooks(Real_GetSystemFirmwareTable_Original);
    InitializeBiosSerialHooks(Real_GetSystemFirmwareTable_Original);
    InitializeProcessorIdHooks(Real_GetSystemFirmwareTable_Original);
    InitializeSystemUuidHooks(Real_GetSystemFirmwareTable_Original);
    return true;
}

void RemoveHardwareHooks() {
    Real_GetSystemFirmwareTable_Original = nullptr; // Clear the pointer
}

PVOID* GetRealGetSystemFirmwareTableAddressStore() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Original);
}

PVOID GetCentralHookFunctionAddress() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable_Central);
}
