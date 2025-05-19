#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void ModifySmbiosForMotherboardSerial(PVOID pFirmwareTableBuffer, DWORD BufferSize);

extern "C" {
    __declspec(dllexport) void SetSpoofedMotherboardSerial(const char* serial);
    __declspec(dllexport) const char* GetOriginalMotherboardSerial();
}

void InitializeMotherboardSerialHooks(PVOID realGetSystemFirmwareTable);
const WCHAR* GetSpoofedMotherboardSerial();