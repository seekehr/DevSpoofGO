#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// --- Function declarations ---

// Hook function
UINT WINAPI Hooked_GetSystemFirmwareTable(DWORD FirmwareTableProviderSignature, DWORD FirmwareTableID, PVOID pFirmwareTableBuffer, DWORD BufferSize);

// API for the injector
extern "C" {
    __declspec(dllexport) void SetSpoofedMotherboardSerial(const char* serial);
    __declspec(dllexport) const char* GetOriginalMotherboardSerial();
}

// Helper functions for hooking from spoof_dll.cpp
PVOID* GetRealGetSystemFirmwareTable();
PVOID GetHookedGetSystemFirmwareTable();

// Access to the spoofed motherboard serial (for other modules if needed)
const WCHAR* GetSpoofedMotherboardSerial(); 