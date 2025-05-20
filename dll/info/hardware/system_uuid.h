#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <guiddef.h>

void ModifySmbiosForSystemUuid(PVOID pFirmwareTableBuffer, DWORD BufferSize);

extern "C" {
    __declspec(dllexport) void SetSpoofedSystemUuid(const char* uuidString);
}

void InitializeSystemUuidHooks(PVOID realGetSystemFirmwareTable);
PVOID* GetRealGetSystemFirmwareTableForUuid();
