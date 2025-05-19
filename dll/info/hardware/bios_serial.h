#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void ModifySmbiosForBiosSerial(PVOID pFirmwareTableBuffer, DWORD BufferSize);

extern "C" {
    __declspec(dllexport) void SetSpoofedBiosSerial(const char* serial);
}

const WCHAR* GetSpoofedBiosSerialNumber();

void InitializeBiosSerialHooks(PVOID realGetSystemFirmwareTable);

PVOID* GetRealGetSystemFirmwareTableForBios();
