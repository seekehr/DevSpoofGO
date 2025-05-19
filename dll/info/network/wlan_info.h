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
DWORD WINAPI Hooked_WlanQueryInterface(HANDLE hClientHandle, const GUID *pInterfaceGuid, WLAN_INTF_OPCODE OpCode, PVOID pReserved, PDWORD pdwDataSize, PVOID *ppData, PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType);
DWORD WINAPI Hooked_WlanGetNetworkBssList(HANDLE hClientHandle, const GUID *pInterfaceGuid, PDOT11_SSID pDot11Ssid, DOT11_BSS_TYPE dot11BssType, BOOL bSecurityEnabled, PVOID pReserved, PWLAN_BSS_LIST *ppWlanBssList);

// API for the injector
extern "C" {
    __declspec(dllexport) void SetSpoofedMacAddress(const char* mac);
    __declspec(dllexport) void SetSpoofedBSSID(const char* bssid);
    __declspec(dllexport) void SetSpoofedWlanGUID(const char* guid);
}

// Helper functions for hooking from spoof_dll.cpp
PVOID* GetRealGetAdaptersInfo();
PVOID GetHookedGetAdaptersInfo();
PVOID* GetRealGetAdaptersAddresses();
PVOID GetHookedGetAdaptersAddresses();
PVOID* GetRealWlanEnumInterfaces();
PVOID GetHookedWlanEnumInterfaces();
PVOID* GetRealWlanQueryInterface();
PVOID GetHookedWlanQueryInterface();
PVOID* GetRealWlanGetNetworkBssList();
PVOID GetHookedWlanGetNetworkBssList();

// Access to the spoofed values for other modules if needed
const WCHAR* GetSpoofedMacAddress();
const WCHAR* GetSpoofedBSSID();
const WCHAR* GetSpoofedWlanGUID();
