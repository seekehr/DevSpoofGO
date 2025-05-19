#include "hardware_info.h"
#include "motherboard_serial.h"
#include "bios_serial.h"
#include "processor_id.h"
#include "system_uuid.h"
#include <windows.h>
#include <cstdio>

static UINT (WINAPI *Real_GetSystemFirmwareTable_Original)(DWORD, DWORD, PVOID, DWORD) = nullptr;

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
        return false;
    }
    InitializeMotherboardSerialHooks(Real_GetSystemFirmwareTable_Original);
    InitializeBiosSerialHooks(Real_GetSystemFirmwareTable_Original);
    InitializeProcessorIdHooks(Real_GetSystemFirmwareTable_Original);
    InitializeSystemUuidHooks(Real_GetSystemFirmwareTable_Original);
    return true;
}

void RemoveHardwareHooks() {
    Real_GetSystemFirmwareTable_Original = nullptr;
}

PVOID* GetRealGetSystemFirmwareTableAddressStore() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Original);
}

PVOID GetCentralHookFunctionAddress() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable_Central);
}
