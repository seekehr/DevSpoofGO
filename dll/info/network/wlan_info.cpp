#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wlanapi.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <tchar.h>
#include "wlan_info.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ws2_32.lib")

// Global variables
WCHAR g_macAddress[32] = L"00:11:22:33:44:55";
WCHAR g_bssid[32] = L"AB:AB:BE:EF:CA:FE";
WCHAR g_wlanGuid[64] = L"{MEOW0F4E-666E-406B-8289-57E05022F3D5}";

// Function pointers to original Windows API functions
static ULONG (WINAPI *Real_GetAdaptersInfo)(PIP_ADAPTER_INFO, PULONG) = GetAdaptersInfo;
static ULONG (WINAPI *Real_GetAdaptersAddresses)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG) = GetAdaptersAddresses;
static DWORD (WINAPI *Real_WlanEnumInterfaces)(HANDLE, PVOID, PWLAN_INTERFACE_INFO_LIST*) = WlanEnumInterfaces;
static DWORD (WINAPI *Real_WlanQueryInterface)(HANDLE, const GUID*, WLAN_INTF_OPCODE, PVOID, PDWORD, PVOID*, PWLAN_OPCODE_VALUE_TYPE) = WlanQueryInterface;
static DWORD (WINAPI *Real_WlanGetNetworkBssList)(HANDLE, const GUID*, PDOT11_SSID, DOT11_BSS_TYPE, BOOL, PVOID, PWLAN_BSS_LIST*) = WlanGetNetworkBssList;

// Helper function prototypes
static void StringToGuid(const char* guidStr, GUID* guid);
static void GuidToString(const GUID* guid, char* guidStr, size_t bufferSize);
static BYTE* ParseMacString(const WCHAR* macStr, BYTE* mac);

// Parse MAC string and convert between GUID formats
static BYTE* ParseMacString(const WCHAR* macStr, BYTE* mac) {
    char ansiMac[32];
    int values[6];
    
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, ansiMac, sizeof(ansiMac), macStr, _TRUNCATE);
    
    if (sscanf_s(ansiMac, "%02x:%02x:%02x:%02x:%02x:%02x", 
               &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) == 6) {
        for (int i = 0; i < 6; i++) mac[i] = static_cast<BYTE>(values[i]);
        return mac;
    }
    
    return NULL;
}

static void StringToGuid(const char* guidStr, GUID* guid) {
    if (!guidStr || strlen(guidStr) < 32) {
        ZeroMemory(guid, sizeof(GUID));
        return;
    }
    
    // Check if we need to strip braces
    const char* parseStr = guidStr;
    char tempGuid[64] = {0};
    
    if (guidStr[0] == '{' && guidStr[strlen(guidStr)-1] == '}') {
        strncpy_s(tempGuid, sizeof(tempGuid), guidStr+1, strlen(guidStr)-2);
        parseStr = tempGuid;
    }
    
    unsigned int data1, data2, data3, data4[8];
    int scanResult = 0;
    
    // Try multiple formats (with/without braces, upper/lowercase)
    scanResult = sscanf_s(parseStr, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             &data1, &data2, &data3, &data4[0], &data4[1], &data4[2], &data4[3],
             &data4[4], &data4[5], &data4[6], &data4[7]);
            
    if (scanResult != 11) {
        scanResult = sscanf_s(parseStr, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                &data1, &data2, &data3, &data4[0], &data4[1], &data4[2], &data4[3],
                &data4[4], &data4[5], &data4[6], &data4[7]);
    }
    
    if (scanResult != 11) {
        ZeroMemory(guid, sizeof(GUID));
        return;
    }
    
    guid->Data1 = data1;
    guid->Data2 = (unsigned short)data2;
    guid->Data3 = (unsigned short)data3;
    for (int i = 0; i < 8; i++) guid->Data4[i] = (unsigned char)data4[i];
}

static void GuidToString(const GUID* guid, char* guidStr, size_t bufferSize) {
    if (bufferSize < 39) {
        if (bufferSize > 0) guidStr[0] = '\0';
        return;
    }
    
    sprintf_s(guidStr, bufferSize, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             guid->Data1, guid->Data2, guid->Data3,
             guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
             guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

// Format GUID without braces for AdapterName (required by Windows APIs)
static void GuidToStringWithoutBraces(const GUID* guid, char* guidStr, size_t bufferSize) {
    if (bufferSize < 37) {
        if (bufferSize > 0) guidStr[0] = '\0';
        return;
    }
    
    sprintf_s(guidStr, bufferSize, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
             guid->Data1, guid->Data2, guid->Data3,
             guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
             guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

// Export functions
extern "C" {
    __declspec(dllexport) void SetSpoofedMacAddress(const char* mac) {
        if (mac) {
            size_t converted = 0;
            mbstowcs_s(&converted, g_macAddress, sizeof(g_macAddress)/sizeof(WCHAR), mac, strlen(mac));
        } 
    }
    
    __declspec(dllexport) void SetSpoofedBSSID(const char* bssid) {
        if (bssid) {
            size_t converted = 0;
            mbstowcs_s(&converted, g_bssid, sizeof(g_bssid)/sizeof(WCHAR), bssid, strlen(bssid));
        }
    }
    
    __declspec(dllexport) void SetSpoofedWlanGUID(const char* guid) {
        if (guid) {
            // First normalize the guid to ensure it has braces if needed
            char normalizedGuid[64] = {0};
            if (guid[0] != '{') {
                sprintf_s(normalizedGuid, sizeof(normalizedGuid), "{%s}", guid);
                if (normalizedGuid[strlen(normalizedGuid)-1] != '}') {
                    normalizedGuid[strlen(normalizedGuid)] = '}';
                }
            } else {
                strcpy_s(normalizedGuid, sizeof(normalizedGuid), guid);
            }
            
            // Now convert to wide string
            size_t converted = 0;
            mbstowcs_s(&converted, g_wlanGuid, sizeof(g_wlanGuid)/sizeof(WCHAR), 
                      normalizedGuid, strlen(normalizedGuid));
            OutputDebugStringW(L"Spoofed WLAN GUID set to: ");
            OutputDebugStringW(g_wlanGuid);
            OutputDebugStringW(L"\n");
        }
    }
}

// Hook for GetAdaptersInfo
ULONG WINAPI Hooked_GetAdaptersInfo(PIP_ADAPTER_INFO AdapterInfo, PULONG SizePointer) {
    ULONG result = Real_GetAdaptersInfo(AdapterInfo, SizePointer);
    
    if (result == NO_ERROR && AdapterInfo != NULL) {
        BYTE spoofedMac[6] = {0};
        if (!ParseMacString(g_macAddress, spoofedMac)) {
            // Default MAC if parsing fails
            spoofedMac[0] = 0x00; spoofedMac[1] = 0x11; spoofedMac[2] = 0x22;
            spoofedMac[3] = 0x33; spoofedMac[4] = 0x44; spoofedMac[5] = 0x55;
        }
        
        for (PIP_ADAPTER_INFO pAdapter = AdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
            memcpy(pAdapter->Address, spoofedMac, 6);
        }
    }
    
    return result;
}

// Hook for GetAdaptersAddresses - now focusing on embedding the BSSID in the description
ULONG WINAPI Hooked_GetAdaptersAddresses(ULONG Family, ULONG Flags, PVOID Reserved, 
                                         PIP_ADAPTER_ADDRESSES AdapterAddresses, PULONG SizePointer) {
    ULONG result = Real_GetAdaptersAddresses(Family, Flags, Reserved, AdapterAddresses, SizePointer);
    
    if (result == NO_ERROR && AdapterAddresses != NULL) {
        BYTE spoofedMac[6] = {0};
        if (!ParseMacString(g_macAddress, spoofedMac)) {
            spoofedMac[0] = 0x00; spoofedMac[1] = 0x11; spoofedMac[2] = 0x22;
            spoofedMac[3] = 0x33; spoofedMac[4] = 0x44; spoofedMac[5] = 0x55;
        }
        
        // GUID for AdapterName (format: MEOW0F4E-666E-406B-8289-57E05022F3D5)
        char guidStr[64];
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, guidStr, sizeof(guidStr), g_wlanGuid, _TRUNCATE);
        
        static WCHAR bssidDescriptionStr[128];
        static WCHAR bssidInNameStr[128];

        swprintf_s(bssidDescriptionStr, _countof(bssidDescriptionStr), L"Wi-Fi BSSID=%s", g_bssid);
        swprintf_s(bssidInNameStr, _countof(bssidInNameStr), L"BSSID-%s", g_bssid);
        
        for (PIP_ADAPTER_ADDRESSES pAdapter = AdapterAddresses; pAdapter; pAdapter = pAdapter->Next) {
            if (pAdapter->PhysicalAddressLength == 6) {
                memcpy(pAdapter->PhysicalAddress, spoofedMac, 6);
                
                if (pAdapter->IfType == 71) { // 71 = IEEE 802.11 wireless
                    pAdapter->AdapterName = guidStr;
                    
                    pAdapter->FriendlyName = bssidInNameStr;
                    pAdapter->Description = bssidDescriptionStr;
                    
                    pAdapter->OperStatus = (IF_OPER_STATUS)1; // IfOperStatusUp
                }
            }
        }
    }
    
    return result;
}

// Hook for WlanEnumInterfaces (revised to ensure at least one interface is present)
DWORD WINAPI Hooked_WlanEnumInterfaces(HANDLE hClientHandle, PVOID pReserved, PWLAN_INTERFACE_INFO_LIST *ppInterfaceList) {
    DWORD result = Real_WlanEnumInterfaces(hClientHandle, pReserved, ppInterfaceList);

    // Convert g_wlanGuid (WCHAR*) to a GUID structure for use below
    GUID currentSpoofedGuid;
    char currentGuidStr[64];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, currentGuidStr, sizeof(currentGuidStr), g_wlanGuid, _TRUNCATE);
    StringToGuid(currentGuidStr, &currentSpoofedGuid); // Assuming StringToGuid handles braces

    if (result == ERROR_SUCCESS) {
        if (*ppInterfaceList == NULL || (*ppInterfaceList)->dwNumberOfItems == 0) {
            if (*ppInterfaceList != NULL) { // If an empty list structure was allocated by the system
                WlanFreeMemory(*ppInterfaceList);
                *ppInterfaceList = NULL;

                size_t requiredSize = offsetof(WLAN_INTERFACE_INFO_LIST, InterfaceInfo) + sizeof(WLAN_INTERFACE_INFO);
                PWLAN_INTERFACE_INFO_LIST newList = (PWLAN_INTERFACE_INFO_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredSize);

                if (newList) {
                    newList->dwNumberOfItems = 1;
                    newList->dwIndex = 0;

                    WLAN_INTERFACE_INFO* pInfo = &(newList->InterfaceInfo[0]);
                    memcpy(&(pInfo->InterfaceGuid), &currentSpoofedGuid, sizeof(GUID));
                    WCHAR descriptionBuffer[256]; // WLAN_MAX_DESCR_LEN is 256
                    swprintf_s(descriptionBuffer, _countof(descriptionBuffer), L"Spoofed Wi-Fi BSSID=%s", g_bssid);
                    wcscpy_s(pInfo->strInterfaceDescription, _countof(pInfo->strInterfaceDescription), descriptionBuffer);
                    pInfo->isState = (WLAN_INTERFACE_STATE)1;

                    *ppInterfaceList = newList;
                } else {
                    // The caller (Go code) should handle *ppInterfaceList being NULL if HeapAlloc fails.
                }
            } else {
                // The caller (Go code) should handle *ppInterfaceList being NULL if HeapAlloc fails.
            }
        } else {
            for (DWORD i = 0; i < (*ppInterfaceList)->dwNumberOfItems; i++) {
                memcpy(&((*ppInterfaceList)->InterfaceInfo[i].InterfaceGuid), &currentSpoofedGuid, sizeof(GUID));
                WCHAR descriptionBuffer[256]; // WLAN_MAX_DESCR_LEN is 256
                swprintf_s(descriptionBuffer, _countof(descriptionBuffer), L"Spoofed Wi-Fi BSSID=%s", g_bssid);
                wcscpy_s((*ppInterfaceList)->InterfaceInfo[i].strInterfaceDescription, _countof((*ppInterfaceList)->InterfaceInfo[i].strInterfaceDescription),
                         descriptionBuffer);
                (*ppInterfaceList)->InterfaceInfo[i].isState = (WLAN_INTERFACE_STATE)1;
            }
        }
    }
    return result;
}

// Hook for WlanQueryInterface (revised to always return spoofed data for current connection)
DWORD WINAPI Hooked_WlanQueryInterface(HANDLE hClientHandle, const GUID *pInterfaceGuid,
                                       WLAN_INTF_OPCODE OpCode, PVOID pReserved,
                                       PDWORD pdwDataSize, PVOID *ppData,
                                       PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType) {
    if (OpCode == wlan_intf_opcode_current_connection) {
        DWORD structSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
        PWLAN_CONNECTION_ATTRIBUTES pConnAttr = (PWLAN_CONNECTION_ATTRIBUTES)WlanAllocateMemory(structSize);

        if (pConnAttr) {
            ZeroMemory(pConnAttr, structSize);

            // Populate with spoofed data
            pConnAttr->isState = (WLAN_INTERFACE_STATE)1; // Connected
            pConnAttr->wlanConnectionMode = wlan_connection_mode_profile;
            wcscpy_s(pConnAttr->strProfileName, _countof(pConnAttr->strProfileName), L"Spoofed WLAN Profile");

            // Association Attributes
            const char* spoofedSsidName = "SpoofedSSID";
            pConnAttr->wlanAssociationAttributes.dot11Ssid.uSSIDLength = (ULONG)strlen(spoofedSsidName);
            memcpy(pConnAttr->wlanAssociationAttributes.dot11Ssid.ucSSID, spoofedSsidName, strlen(spoofedSsidName));
            pConnAttr->wlanAssociationAttributes.dot11BssType = dot11_BSS_type_infrastructure;
            
            BYTE spoofedBssid[6] = {0};
            if (!ParseMacString(g_bssid, spoofedBssid)) {
                // Fallback if g_bssid is somehow invalid or parsing fails
                spoofedBssid[0] = 0xDE; spoofedBssid[1] = 0xAD; spoofedBssid[2] = 0xBE;
                spoofedBssid[3] = 0xEF; spoofedBssid[4] = 0xCA; spoofedBssid[5] = 0xFE;
            }
            memcpy(pConnAttr->wlanAssociationAttributes.dot11Bssid, spoofedBssid, 6);
            
            pConnAttr->wlanAssociationAttributes.wlanSignalQuality = 100;

            *pdwDataSize = structSize;
            *ppData = pConnAttr; // The caller (WlanAPI client) is responsible for freeing this with WlanFreeMemory

            return ERROR_SUCCESS;
        } else {
            if (pdwDataSize) *pdwDataSize = 0;
            if (ppData) *ppData = NULL;
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    return Real_WlanQueryInterface(hClientHandle, pInterfaceGuid, OpCode, pReserved,
                                   pdwDataSize, ppData, pWlanOpcodeValueType);
}

// Hook for WlanGetNetworkBssList to spoof BSS list
DWORD WINAPI Hooked_WlanGetNetworkBssList(HANDLE hClientHandle, const GUID *pInterfaceGuid,
                                         PDOT11_SSID pDot11Ssid, DOT11_BSS_TYPE dot11BssType,
                                         BOOL bSecurityEnabled, PVOID pReserved,
                                         PWLAN_BSS_LIST *ppWlanBssList) {
    DWORD result = Real_WlanGetNetworkBssList(hClientHandle, pInterfaceGuid, pDot11Ssid, 
                                             dot11BssType, bSecurityEnabled, pReserved, 
                                             ppWlanBssList);
    
    if (result == ERROR_SUCCESS && *ppWlanBssList != NULL) {
        BYTE spoofedBssid[6] = {0};
        if (!ParseMacString(g_bssid, spoofedBssid)) {
            // Default BSSID if parsing fails
            spoofedBssid[0] = 0xAA; spoofedBssid[1] = 0xBB; spoofedBssid[2] = 0xCC;
            spoofedBssid[3] = 0xDD; spoofedBssid[4] = 0xEE; spoofedBssid[5] = 0xFF;
        }
        
        for (DWORD i = 0; i < (*ppWlanBssList)->dwNumberOfItems && i < 10; i++) {
            memcpy((*ppWlanBssList)->wlanBssEntries[i].dot11Bssid, spoofedBssid, 6);
            // Increment the last byte to make each BSSID slightly different for a list
            if (i < 9) { // Avoid overflow on the last iteration if we spoof many
                 spoofedBssid[5]++;
            }
        }
    }
    
    return result;
}

// Helper functions for Detours hooking
PVOID* GetRealGetAdaptersInfo() { return reinterpret_cast<PVOID*>(&Real_GetAdaptersInfo); }
PVOID GetHookedGetAdaptersInfo() { return reinterpret_cast<PVOID>(Hooked_GetAdaptersInfo); }
PVOID* GetRealGetAdaptersAddresses() { return reinterpret_cast<PVOID*>(&Real_GetAdaptersAddresses); }
PVOID GetHookedGetAdaptersAddresses() { return reinterpret_cast<PVOID>(Hooked_GetAdaptersAddresses); }
PVOID* GetRealWlanEnumInterfaces() { return reinterpret_cast<PVOID*>(&Real_WlanEnumInterfaces); }
PVOID GetHookedWlanEnumInterfaces() { return reinterpret_cast<PVOID>(Hooked_WlanEnumInterfaces); }
PVOID* GetRealWlanQueryInterface() { return reinterpret_cast<PVOID*>(&Real_WlanQueryInterface); }
PVOID GetHookedWlanQueryInterface() { return reinterpret_cast<PVOID>(Hooked_WlanQueryInterface); }
PVOID* GetRealWlanGetNetworkBssList() { return reinterpret_cast<PVOID*>(&Real_WlanGetNetworkBssList); }
PVOID GetHookedWlanGetNetworkBssList() { return reinterpret_cast<PVOID>(Hooked_WlanGetNetworkBssList); }

// Access functions
const WCHAR* GetSpoofedMacAddress() { return g_macAddress; }
const WCHAR* GetSpoofedBSSID() { return g_bssid; }
const WCHAR* GetSpoofedWlanGUID() { return g_wlanGuid; }
