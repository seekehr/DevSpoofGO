#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h> // Must come before iphlpapi.h
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ipifcons.h> // For MIB_IF_TYPE_IEEE80211
#include <wlanapi.h>
#include <cstring>
#include <cstdio>
#include <random>
#include "wlan_info.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ws2_32.lib")

// --- Network information related global variables ---
// Store the spoofed MAC address
WCHAR g_macAddress[32] = L"00:11:22:33:44:55"; // Default value

// --- Function pointers to original Windows API functions ---
static ULONG (WINAPI *Real_GetAdaptersInfo)(PIP_ADAPTER_INFO, PULONG) = GetAdaptersInfo;
static ULONG (WINAPI *Real_GetAdaptersAddresses)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG) = GetAdaptersAddresses;
static DWORD (WINAPI *Real_WlanEnumInterfaces)(HANDLE, PVOID, PWLAN_INTERFACE_INFO_LIST*) = WlanEnumInterfaces;

// --- Forward declarations ---
static const char* Internal_GetOriginalMacAddress();
static void GenerateRandomMacAddress(BYTE* mac);

// --- Function to get the original MAC address ---
static const char* Internal_GetOriginalMacAddress() {
    OutputDebugStringW(L"Internal_GetOriginalMacAddress called");
    static char macBuffer[64] = {0};
    
    IP_ADAPTER_INFO* pAdapterInfo = NULL;
    ULONG ulOutBufLen = 0;
    
    // First call to get the required buffer size
    DWORD result = Real_GetAdaptersInfo(NULL, &ulOutBufLen);
    if (result != ERROR_BUFFER_OVERFLOW) {
        return "ERROR_BUFFER_SIZE";
    }
    
    // Allocate memory for adapter info
    pAdapterInfo = (IP_ADAPTER_INFO*)new BYTE[ulOutBufLen];
    if (!pAdapterInfo) {
        return "ERROR_MEMORY";
    }
    
    // Get adapter info
    result = Real_GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (result != NO_ERROR) {
        delete[] pAdapterInfo;
        return "ERROR_GET_ADAPTERS";
    }
    
    // Find first wireless adapter
    IP_ADAPTER_INFO* pAdapter = pAdapterInfo;
    bool found = false;
    
    while (pAdapter) {
        // Check if this is a wireless adapter (simplified check)
        if (pAdapter->Type == 71) { // 71 is the value for IEEE 802.11 wireless interfaces
            // Format MAC address as XX:XX:XX:XX:XX:XX
            sprintf_s(macBuffer, sizeof(macBuffer), 
                     "%02X:%02X:%02X:%02X:%02X:%02X",
                     pAdapter->Address[0], pAdapter->Address[1], 
                     pAdapter->Address[2], pAdapter->Address[3], 
                     pAdapter->Address[4], pAdapter->Address[5]);
            found = true;
            break;
        }
        pAdapter = pAdapter->Next;
    }
    
    if (!found && pAdapterInfo) {
        // If no wireless adapter, use the first one
        sprintf_s(macBuffer, sizeof(macBuffer), 
                 "%02X:%02X:%02X:%02X:%02X:%02X",
                 pAdapterInfo->Address[0], pAdapterInfo->Address[1], 
                 pAdapterInfo->Address[2], pAdapterInfo->Address[3], 
                 pAdapterInfo->Address[4], pAdapterInfo->Address[5]);
    }
    
    // Clean up
    delete[] pAdapterInfo;
    
    return macBuffer;
}

// --- Function for export ---
extern "C" {
    __declspec(dllexport) const char* GetOriginalMacAddress() {
        return Internal_GetOriginalMacAddress();
    }
}

// --- Generate random MAC address ---
static void GenerateRandomMacAddress(BYTE* mac) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (int i = 0; i < 6; i++) {
        mac[i] = static_cast<BYTE>(dis(gen));
    }
    
    // Ensure it's a locally administered address (set bit 1 of first byte)
    // and not a multicast address (clear bit 0 of first byte)
    mac[0] = (mac[0] & 0xFC) | 0x02;
}

// --- Hook function for GetAdaptersInfo ---
ULONG WINAPI Hooked_GetAdaptersInfo(PIP_ADAPTER_INFO AdapterInfo, PULONG SizePointer) {
    // Debug output
    OutputDebugStringW(L"Hooked_GetAdaptersInfo called");
    
    // Call the original function
    ULONG result = Real_GetAdaptersInfo(AdapterInfo, SizePointer);
    
    // Only modify if the call was successful and we have adapter info
    if (result == NO_ERROR && AdapterInfo != NULL) {
        // Convert spoofed MAC to bytes
        BYTE spoofedMac[6] = {0};
        int values[6];
        char ansiMac[32];
        
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, ansiMac, sizeof(ansiMac), g_macAddress, _TRUNCATE);
        
        // Parse the MAC string
        if (sscanf_s(ansiMac, "%02x:%02x:%02x:%02x:%02x:%02x", 
                    &values[0], &values[1], &values[2], 
                    &values[3], &values[4], &values[5]) == 6) {
            for (int i = 0; i < 6; i++) {
                spoofedMac[i] = static_cast<BYTE>(values[i]);
            }
        } else {
            // If parsing failed, generate a random MAC
            GenerateRandomMacAddress(spoofedMac);
        }
        
        // Walk through all adapters and modify their MAC addresses
        PIP_ADAPTER_INFO pAdapter = AdapterInfo;
        while (pAdapter) {
            // Replace the MAC address
            memcpy(pAdapter->Address, spoofedMac, 6);
            
            // For wireless adapters, we can also modify other specific fields
            if (pAdapter->Type == 71) { // 71 is the value for IEEE 802.11 wireless interfaces
                // You could modify additional wireless-specific fields here
            }
            
            pAdapter = pAdapter->Next;
        }
        
        OutputDebugStringW(L"Successfully spoofed adapter MAC addresses");
    } else {
        WCHAR debugMsg[256];
        swprintf_s(debugMsg, _countof(debugMsg), L"GetAdaptersInfo returned error: %u", result);
        OutputDebugStringW(debugMsg);
    }
    
    return result;
}

// --- Hook function for GetAdaptersAddresses ---
ULONG WINAPI Hooked_GetAdaptersAddresses(ULONG Family, 
                                          ULONG Flags, 
                                          PVOID Reserved, 
                                          PIP_ADAPTER_ADDRESSES AdapterAddresses, 
                                          PULONG SizePointer) {
    // Debug output
    OutputDebugStringW(L"Hooked_GetAdaptersAddresses called");
    
    // Call the original function
    ULONG result = Real_GetAdaptersAddresses(Family, Flags, Reserved, AdapterAddresses, SizePointer);
    
    // Only modify if the call was successful and we have adapter info
    if (result == NO_ERROR && AdapterAddresses != NULL) {
        // Convert spoofed MAC to bytes
        BYTE spoofedMac[6] = {0};
        int values[6];
        char ansiMac[32];
        
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, ansiMac, sizeof(ansiMac), g_macAddress, _TRUNCATE);
        
        // Parse the MAC string
        if (sscanf_s(ansiMac, "%02x:%02x:%02x:%02x:%02x:%02x", 
                    &values[0], &values[1], &values[2], 
                    &values[3], &values[4], &values[5]) == 6) {
            for (int i = 0; i < 6; i++) {
                spoofedMac[i] = static_cast<BYTE>(values[i]);
            }
        } else {
            // If parsing failed, generate a random MAC
            GenerateRandomMacAddress(spoofedMac);
        }
        
        // Walk through all adapters and modify their MAC addresses
        PIP_ADAPTER_ADDRESSES pAdapter = AdapterAddresses;
        while (pAdapter) {
            // Replace the MAC address (PhysicalAddress)
            if (pAdapter->PhysicalAddressLength == 6) {
                memcpy(pAdapter->PhysicalAddress, spoofedMac, 6);
            }
            
            // Modify IP addresses if desired
            // This would require additional code to handle IP_ADAPTER_UNICAST_ADDRESS structures
            
            pAdapter = pAdapter->Next;
        }
        
        OutputDebugStringW(L"Successfully spoofed adapter addresses");
    } else {
        WCHAR debugMsg[256];
        swprintf_s(debugMsg, _countof(debugMsg), L"GetAdaptersAddresses returned error: %u", result);
        OutputDebugStringW(debugMsg);
    }
    
    return result;
}

// --- Hook function for WlanEnumInterfaces ---
DWORD WINAPI Hooked_WlanEnumInterfaces(HANDLE hClientHandle, 
                                        PVOID pReserved, 
                                        PWLAN_INTERFACE_INFO_LIST *ppInterfaceList) {
    // Debug output
    OutputDebugStringW(L"Hooked_WlanEnumInterfaces called");
    
    // Call the original function
    DWORD result = Real_WlanEnumInterfaces(hClientHandle, pReserved, ppInterfaceList);
    
    // Only modify if the call was successful and we have interface info
    if (result == ERROR_SUCCESS && *ppInterfaceList != NULL) {
        // Convert spoofed MAC to bytes for potential use
        BYTE spoofedMac[6] = {0};
        int values[6];
        char ansiMac[32];
        
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, ansiMac, sizeof(ansiMac), g_macAddress, _TRUNCATE);
        
        // Parse the MAC string
        if (sscanf_s(ansiMac, "%02x:%02x:%02x:%02x:%02x:%02x", 
                    &values[0], &values[1], &values[2], 
                    &values[3], &values[4], &values[5]) == 6) {
            for (int i = 0; i < 6; i++) {
                spoofedMac[i] = static_cast<BYTE>(values[i]);
            }
        } else {
            // If parsing failed, generate a random MAC
            GenerateRandomMacAddress(spoofedMac);
        }
        
        // We can modify interface specific information here
        for (DWORD i = 0; i < (*ppInterfaceList)->dwNumberOfItems; i++) {
            // For example, modify the interface description or other non-critical info
            // Note: Direct MAC modification is not possible here, as it's not part of WLAN_INTERFACE_INFO
            
            // Create a spoofed interface name that might include a random element
            WCHAR randomPart[8];
            swprintf_s(randomPart, _countof(randomPart), L"%04X", GetTickCount() % 0xFFFF);
            
            // WLAN_INTERFACE_INFO structure doesn't contain MAC directly,
            // but we can modify visible fields like the description
            wcscpy_s((*ppInterfaceList)->InterfaceInfo[i].strInterfaceDescription, 
                     L"Wi-Fi Adapter");
        }
        
        OutputDebugStringW(L"Successfully processed WLAN interfaces");
    } else {
        WCHAR debugMsg[256];
        swprintf_s(debugMsg, _countof(debugMsg), L"WlanEnumInterfaces returned error: %u", result);
        OutputDebugStringW(debugMsg);
    }
    
    return result;
}

// --- Function to set the spoofed MAC address ---
void SetSpoofedMacAddress(const char* mac) {
    if (mac) {
        // Convert the input ANSI string (char*) to a wide character string (WCHAR*)
        const size_t len = strlen(mac) + 1; // Include space for null terminator
        size_t converted = 0;
        // Use mbstowcs_s for secure conversion to WCHAR
        mbstowcs_s(&converted, g_macAddress, sizeof(g_macAddress) / sizeof(WCHAR), 
                  mac, len - 1);
        // mbstowcs_s automatically null-terminates if the source fits.
    } else {
        // If no MAC provided, generate a random one
        BYTE randomMac[6];
        GenerateRandomMacAddress(randomMac);
        
        swprintf_s(g_macAddress, _countof(g_macAddress), 
                  L"%02X:%02X:%02X:%02X:%02X:%02X",
                  randomMac[0], randomMac[1], randomMac[2],
                  randomMac[3], randomMac[4], randomMac[5]);
    }
}

// --- Helper functions for hooking ---
PVOID* GetRealGetAdaptersInfo() {
    return reinterpret_cast<PVOID*>(&Real_GetAdaptersInfo);
}

PVOID GetHookedGetAdaptersInfo() {
    return reinterpret_cast<PVOID>(Hooked_GetAdaptersInfo);
}

PVOID* GetRealGetAdaptersAddresses() {
    return reinterpret_cast<PVOID*>(&Real_GetAdaptersAddresses);
}

PVOID GetHookedGetAdaptersAddresses() {
    return reinterpret_cast<PVOID>(Hooked_GetAdaptersAddresses);
}

PVOID* GetRealWlanEnumInterfaces() {
    return reinterpret_cast<PVOID*>(&Real_WlanEnumInterfaces);
}

PVOID GetHookedWlanEnumInterfaces() {
    return reinterpret_cast<PVOID>(Hooked_WlanEnumInterfaces);
}

// --- Function to get the spoofed MAC address for other modules ---
const WCHAR* GetSpoofedMacAddress() {
    return g_macAddress;
}
