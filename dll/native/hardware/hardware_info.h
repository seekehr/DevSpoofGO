#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "bios_serial.h"
#include "processor_id.h"
#include "system_uuid.h"

UINT WINAPI Hooked_GetSystemFirmwareTable_Central(DWORD FirmwareTableProviderSignature, 
                                                DWORD FirmwareTableID, 
                                                PVOID pFirmwareTableBuffer, 
                                                DWORD BufferSize);

bool InitializeHardwareHooks();
void RemoveHardwareHooks();
PVOID* GetRealGetSystemFirmwareTableAddressStore();
PVOID GetCentralHookFunctionAddress();
