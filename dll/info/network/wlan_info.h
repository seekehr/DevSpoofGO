#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h> // Must come before iphlpapi.h
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wlanapi.h>

// Hook functions
ULONG WINAPI Hooked_GetAdaptersInfo(PIP_ADAPTER_INFO AdapterInfo, PULONG SizePointer);
ULONG WINAPI Hooked_GetAdaptersAddresses(ULONG Family, ULONG Flags, PVOID Reserved, PIP_ADAPTER_ADDRESSES AdapterAddresses, PULONG SizePointer);
DWORD WINAPI Hooked_WlanEnumInterfaces(HANDLE hClientHandle, PVOID pReserved, PWLAN_INTERFACE_INFO_LIST *ppInterfaceList);

// API for the injector
extern "C" {
    __declspec(dllexport) void SetSpoofedMacAddress(const char* mac);
    __declspec(dllexport) const char* GetOriginalMacAddress();
}

// Helper functions for hooking from spoof_dll.cpp
PVOID* GetRealGetAdaptersInfo();
PVOID GetHookedGetAdaptersInfo();
PVOID* GetRealGetAdaptersAddresses();
PVOID GetHookedGetAdaptersAddresses();
PVOID* GetRealWlanEnumInterfaces();
PVOID GetHookedWlanEnumInterfaces();

// Access to the spoofed MAC address (for other modules if needed)
const WCHAR* GetSpoofedMacAddress();
