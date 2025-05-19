#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <guiddef.h> // For GUID definition

// Function to modify SMBIOS data to change the System UUID
void ModifySmbiosForSystemUuid(PVOID pFirmwareTableBuffer, DWORD BufferSize);

// Extern "C" to ensure C-style linkage for dllexport
extern "C" {
    // Function to set the spoofed System UUID from a string
    __declspec(dllexport) void SetSpoofedSystemUuid(const char* uuidString);
}

// Function to retrieve the currently set spoofed System UUID
const GUID& GetSpoofedSystemUuid();

// Function to initialize hooks related to System UUID, primarily GetSystemFirmwareTable
// Takes a pointer to the real GetSystemFirmwareTable function
void InitializeSystemUuidHooks(PVOID realGetSystemFirmwareTable);

// Function to get the storage location for the real GetSystemFirmwareTable function pointer
// specific to System UUID modification (if needed, or can use a central one)
PVOID* GetRealGetSystemFirmwareTableForUuid();
