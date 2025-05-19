#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// --- Function declarations for centralized hooking ---

// The main hooked version of GetSystemFirmwareTable
UINT WINAPI Hooked_GetSystemFirmwareTable_Central(DWORD FirmwareTableProviderSignature, 
                                                DWORD FirmwareTableID, 
                                                PVOID pFirmwareTableBuffer, 
                                                DWORD BufferSize);

// Function to initialize all hardware info hooks
// This function will obtain the real GetSystemFirmwareTable address
// and pass it to the individual modules (bios_serial, motherboard_serial)
bool InitializeHardwareHooks();

// Function to remove all hardware info hooks
void RemoveHardwareHooks();

// Function to get the address of the real GetSystemFirmwareTable, to be stored by the hook manager
PVOID* GetRealGetSystemFirmwareTableAddressStore();

// Function to get the address of our central hook function
PVOID GetCentralHookFunctionAddress();
