#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// --- Function declarations ---

// Function to modify SMBIOS data for motherboard serial
void ModifySmbiosForMotherboardSerial(PVOID pFirmwareTableBuffer, DWORD BufferSize);

// API for the injector
extern "C" {
    __declspec(dllexport) void SetSpoofedMotherboardSerial(const char* serial);
    __declspec(dllexport) const char* GetOriginalMotherboardSerial();
}

// Helper functions for hooking from spoof_dll.cpp
// PVOID* GetRealGetSystemFirmwareTable();  // To be removed
// PVOID GetHookedGetSystemFirmwareTable(); // To be removed

// Function to initialize and get the original motherboard serial
void InitializeMotherboardSerialHooks(PVOID realGetSystemFirmwareTable);

// Access to the spoofed motherboard serial (for other modules if needed)
const WCHAR* GetSpoofedMotherboardSerial(); 