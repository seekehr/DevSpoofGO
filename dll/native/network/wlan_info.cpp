#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#include <wlanapi.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <tchar.h>
#include "wlan_info.h"
#include "../../detours/include/detours.h"

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ws2_32.lib")

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
    
    const char* parseStr = guidStr;
    char tempGuid[64] = {0};
    
    if (guidStr[0] == '{' && guidStr[strlen(guidStr)-1] == '}') {
        strncpy_s(tempGuid, sizeof(tempGuid), guidStr+1, strlen(guidStr)-2);
        parseStr = tempGuid;
    }
    
    unsigned int data1, data2, data3, data4[8];
    int scanResult = 0;
    
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
    
    __declspec(dllexport) void SetSpoofedWlanGUID(const char* guidInput) {
        if (guidInput && strlen(guidInput) > 0) {
            char coreGuidStr[64] = {0}; // To store the GUID part without braces
            size_t inputLen = strlen(guidInput);

            // Attempt to extract the core GUID string if input has braces
            if (inputLen > 2 && guidInput[0] == '{' && guidInput[inputLen - 1] == '}') {
                strncpy_s(coreGuidStr, sizeof(coreGuidStr), guidInput + 1, inputLen - 2);
            } else {
                // Assume input is already the core GUID string (or has mismatched/no braces)
                strncpy_s(coreGuidStr, sizeof(coreGuidStr), guidInput, inputLen);
            }

            // Now, ensure g_wlanGuid is stored with braces {CORE_GUID_STRING}
            char finalGuidWithBraces[64];
            if (strlen(coreGuidStr) > 0) {
                sprintf_s(finalGuidWithBraces, sizeof(finalGuidWithBraces), "{%s}", coreGuidStr);
            } else {
                // If core is empty (e.g. input was "{}" or ""), store as "{}"
                // StringToGuid should handle this by zeroing the GUID struct.
                strcpy_s(finalGuidWithBraces, sizeof(finalGuidWithBraces), "{}");
            }
            
            size_t converted = 0;
            mbstowcs_s(&converted, g_wlanGuid, sizeof(g_wlanGuid)/sizeof(WCHAR), 
                      finalGuidWithBraces, strlen(finalGuidWithBraces));
        }
    }
}

ULONG WINAPI Hooked_GetAdaptersInfo(PIP_ADAPTER_INFO AdapterInfo, PULONG SizePointer) {
    OutputDebugStringW(L"Hooked_GetAdaptersInfo called");
    // AdapterName and Description are fixed char arrays within the struct.
    ULONG requiredSize = sizeof(IP_ADAPTER_INFO);

    if (SizePointer == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (AdapterInfo == NULL || *SizePointer < requiredSize) {
        *SizePointer = requiredSize;
        return ERROR_BUFFER_OVERFLOW;
    }

    // Buffer is provided and sufficient. Zero it and populate.
    ZeroMemory(AdapterInfo, requiredSize);

    // Spoofed MAC address
    BYTE spoofedMac[6];
    if (!ParseMacString(g_macAddress, spoofedMac)) {
        // Default fallback if g_macAddress is somehow invalid
        spoofedMac[0] = 0x00; spoofedMac[1] = 0x11; spoofedMac[2] = 0x22;
        spoofedMac[3] = 0x33; spoofedMac[4] = 0x44; spoofedMac[5] = 0x55;
    }

    // Adapter Name (GUID, typically with braces for this API)
    char adapterNameGuidStr[64];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, adapterNameGuidStr, sizeof(adapterNameGuidStr), g_wlanGuid, _TRUNCATE);
    strncpy_s(AdapterInfo->AdapterName, sizeof(AdapterInfo->AdapterName), adapterNameGuidStr, _TRUNCATE);
    
    // Description
    WCHAR descriptionBuffer[256];
    swprintf_s(descriptionBuffer, _countof(descriptionBuffer), L"Spoofed Wireless Adapter BSSID=%s", g_bssid);
    char descriptionAnsi[256];
    wcstombs_s(&convertedChars, descriptionAnsi, sizeof(descriptionAnsi), descriptionBuffer, _TRUNCATE);
    strncpy_s(AdapterInfo->Description, sizeof(AdapterInfo->Description), descriptionAnsi, _TRUNCATE);

    AdapterInfo->AddressLength = 6;
    memcpy(AdapterInfo->Address, spoofedMac, 6);

    AdapterInfo->Index = 1; // Arbitrary, but consistent, index
    AdapterInfo->Type = 71; // MIB_IF_TYPE_IEEE80211 for Wireless

    AdapterInfo->DhcpEnabled = TRUE;
    AdapterInfo->HaveWins = FALSE; // Typically false for modern setups

    // IP Address, Subnet Mask, Gateway - set to 0.0.0.0 to indicate not configured or disconnected
    // These are IP_ADDR_STRING structs embedded in IP_ADAPTER_INFO
    strcpy_s(AdapterInfo->IpAddressList.IpAddress.String, sizeof(AdapterInfo->IpAddressList.IpAddress.String), "0.0.0.0");
    strcpy_s(AdapterInfo->IpAddressList.IpMask.String, sizeof(AdapterInfo->IpAddressList.IpMask.String), "0.0.0.0");
    AdapterInfo->IpAddressList.Context = 0; // Or some other placeholder
    AdapterInfo->IpAddressList.Next = NULL;

    strcpy_s(AdapterInfo->GatewayList.IpAddress.String, sizeof(AdapterInfo->GatewayList.IpAddress.String), "0.0.0.0");
    // GatewayList's mask isn't typically used directly like this, but fill for completeness
    strcpy_s(AdapterInfo->GatewayList.IpMask.String, sizeof(AdapterInfo->GatewayList.IpMask.String), "0.0.0.0");
    AdapterInfo->GatewayList.Context = 0;
    AdapterInfo->GatewayList.Next = NULL;

    // DHCP Server IP
    strcpy_s(AdapterInfo->DhcpServer.IpAddress.String, sizeof(AdapterInfo->DhcpServer.IpAddress.String), "0.0.0.0");
    strcpy_s(AdapterInfo->DhcpServer.IpMask.String, sizeof(AdapterInfo->DhcpServer.IpMask.String), "0.0.0.0"); // Mask usually 255.255.255.255 for a server
    AdapterInfo->DhcpServer.Context = 0;
    AdapterInfo->DhcpServer.Next = NULL;
    
    // Lease times (can be set to 0 or a fixed spoofed time)
    AdapterInfo->LeaseObtained = 0; 
    AdapterInfo->LeaseExpires = 0;

    // Primary/Secondary WINS servers (usually not used)
    strcpy_s(AdapterInfo->PrimaryWinsServer.IpAddress.String, sizeof(AdapterInfo->PrimaryWinsServer.IpAddress.String), "0.0.0.0");
    strcpy_s(AdapterInfo->SecondaryWinsServer.IpAddress.String, sizeof(AdapterInfo->SecondaryWinsServer.IpAddress.String), "0.0.0.0");

    AdapterInfo->Next = NULL; // Only one adapter in the list

    *SizePointer = requiredSize; // The amount of data written
    return NO_ERROR;
}

// Hook for GetAdaptersAddresses - now focusing on embedding the BSSID in the description
ULONG WINAPI Hooked_GetAdaptersAddresses(ULONG Family, ULONG Flags, PVOID Reserved,
                                         PIP_ADAPTER_ADDRESSES AdapterAddresses, PULONG SizePointer) {
    OutputDebugStringW(L"Hooked_GetAdaptersAddresses called");
    // Define lengths for our strings to calculate buffer size
    // Max length for GUID string (e.g., "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}") is 38 + null terminator
    // AdapterName for IP_ADAPTER_ADDRESSES is char* and typically without braces.
    const size_t adapterNameStrLen = 36 + 1; // XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
    const size_t friendlyNameMaxLen = 128; // WCHARs
    const size_t descriptionMaxLen = 256;  // WCHARs

    // Calculate the base size for the IP_ADAPTER_ADDRESSES structure itself.
    ULONG requiredSize = sizeof(IP_ADAPTER_ADDRESSES);
    // Add space for the strings that will be stored immediately after the structure.
    // These pointers in the struct will point to these memory locations.
    requiredSize += (adapterNameStrLen * sizeof(char));      // For AdapterName
    requiredSize += (friendlyNameMaxLen * sizeof(WCHAR));  // For FriendlyName
    requiredSize += (descriptionMaxLen * sizeof(WCHAR));   // For Description
    
    // Ensure alignment, though for this sequence it's often naturally aligned.
    requiredSize = (requiredSize + MEMORY_ALLOCATION_ALIGNMENT - 1) & ~(MEMORY_ALLOCATION_ALIGNMENT - 1);

    if (SizePointer == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (AdapterAddresses == NULL || *SizePointer < requiredSize) {
        *SizePointer = requiredSize;
        // If AdapterAddresses is NULL, it's a size query, so ERROR_SUCCESS or ERROR_BUFFER_OVERFLOW based on docs.
        // For GetAdaptersAddresses, if pAdapterAddresses is NULL and pulSize is non-zero, it's an error (ERROR_INVALID_PARAMETER).
        // If pAdapterAddresses is NULL and pulSize is 0, it should return ERROR_BUFFER_OVERFLOW with required size.
        // We simplify: if buffer is too small (or NULL), always return ERROR_BUFFER_OVERFLOW.
        return ERROR_BUFFER_OVERFLOW;
    }

    // Buffer is provided and sufficient. Zero it and populate.
    ZeroMemory(AdapterAddresses, *SizePointer); // Zero the whole provided buffer for safety
    PIP_ADAPTER_ADDRESSES pCurrent = AdapterAddresses;

    // Set up pointers for the strings that will follow the main struct
    char* currentBufferPtr = (char*)pCurrent + sizeof(IP_ADAPTER_ADDRESSES);

    pCurrent->AdapterName = (char*)currentBufferPtr;
    currentBufferPtr += (adapterNameStrLen * sizeof(char));

    pCurrent->FriendlyName = (PWCHAR)currentBufferPtr;
    currentBufferPtr += (friendlyNameMaxLen * sizeof(WCHAR));

    pCurrent->Description = (PWCHAR)currentBufferPtr;

    pCurrent->Length = requiredSize; // Own length
    pCurrent->IfIndex = 1; // Arbitrary, but consistent, interface index (LUID will be derived or zeroed)

    // Adapter Name (GUID without braces)
    char adapterNameGuidStr[adapterNameStrLen];
    GUID tempGuid;
    char tempGuidStr[64];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, tempGuidStr, sizeof(tempGuidStr), g_wlanGuid, _TRUNCATE);
    StringToGuid(tempGuidStr, &tempGuid); // Convert WCHAR g_wlanGuid (potentially with braces) to GUID
    GuidToStringWithoutBraces(&tempGuid, adapterNameGuidStr, sizeof(adapterNameGuidStr)); // Format GUID to string without braces
    strcpy_s(pCurrent->AdapterName, adapterNameStrLen, adapterNameGuidStr);

    // Physical Address (MAC)
    BYTE spoofedMac[6];
    if (!ParseMacString(g_macAddress, spoofedMac)) {
        spoofedMac[0] = 0x00; spoofedMac[1] = 0x11; spoofedMac[2] = 0x22;
        spoofedMac[3] = 0x33; spoofedMac[4] = 0x44; spoofedMac[5] = 0x55;
    }
    pCurrent->PhysicalAddressLength = 6;
    memcpy(pCurrent->PhysicalAddress, spoofedMac, 6);

    // Friendly Name and Description (BSSID embedded)
    swprintf_s(pCurrent->FriendlyName, friendlyNameMaxLen, L"Spoofed WLAN BSSID-%s", g_bssid);
    swprintf_s(pCurrent->Description, descriptionMaxLen, L"Spoofed Wireless LAN adapter Wi-Fi BSSID=%s", g_bssid);

    pCurrent->IfType = 71; // MIB_IF_TYPE_IEEE80211 / IF_TYPE_IEEE80211_RADIO for Wireless
    pCurrent->OperStatus = IfOperStatusUp;
    pCurrent->Flags = IP_ADAPTER_DHCP_ENABLED | IP_ADAPTER_RECEIVE_ONLY; // Example flags

    // GUID for the adapter - LUID is used for this
    // Convert the WCHAR g_wlanGuid to a GUID structure first
    GUID interfaceGuid;
    char currentGuidStr[64];
    size_t convertedCharsLuid = 0;
    wcstombs_s(&convertedCharsLuid, currentGuidStr, sizeof(currentGuidStr), g_wlanGuid, _TRUNCATE);
    StringToGuid(currentGuidStr, &interfaceGuid); // StringToGuid should handle braces in g_wlanGuid

    // Let's ensure it's zeroed initially and then set IfType for consistency.
    ZeroMemory(&pCurrent->Luid, sizeof(IF_LUID));
    pCurrent->Luid.Info.IfType = pCurrent->IfType; // Set the IfType part of the LUID info.
    // If a specific LUID value is required, it needs more complex handling.

    // Set IP address related fields to NULL or empty as we are not connected/configured
    pCurrent->FirstUnicastAddress = NULL;
    pCurrent->FirstAnycastAddress = NULL;
    pCurrent->FirstMulticastAddress = NULL;
    pCurrent->FirstWinsServerAddress = NULL;
    pCurrent->FirstGatewayAddress = NULL;
    pCurrent->FirstDnsServerAddress = NULL; // Important for name resolution spoofing if needed

    pCurrent->DnsSuffix = NULL; // Or L""
    pCurrent->Ipv6IfIndex = 0; // If not supporting IPv6 specifically here

    // Transmit/Receive link speeds (can be spoofed to typical Wi-Fi values if needed)
    pCurrent->TransmitLinkSpeed = 866700000; // Example: 866.7 Mbps
    pCurrent->ReceiveLinkSpeed = 866700000;  // Example: 866.7 Mbps

    pCurrent->Next = NULL; // Only one adapter in the list

    *SizePointer = requiredSize; // The amount of data written
    return NO_ERROR;
}

// Hook for WlanEnumInterfaces (revised to ensure at least one interface is present)
DWORD WINAPI Hooked_WlanEnumInterfaces(HANDLE hClientHandle, PVOID pReserved, PWLAN_INTERFACE_INFO_LIST *ppInterfaceList) {
    OutputDebugStringW(L"Hooked_WlanEnumInterfaces called");
    DWORD result = Real_WlanEnumInterfaces(hClientHandle, pReserved, ppInterfaceList);

    GUID currentSpoofedGuid;
    char currentGuidStr[64];
    size_t convertedChars = 0;
    wcstombs_s(&convertedChars, currentGuidStr, sizeof(currentGuidStr), g_wlanGuid, _TRUNCATE);
    StringToGuid(currentGuidStr, &currentSpoofedGuid); // Assuming StringToGuid handles braces

    if (result == ERROR_SUCCESS) {
        // If the real call was successful, we discard its results and create our own single spoofed interface.
        if (*ppInterfaceList != NULL) {
            WlanFreeMemory(*ppInterfaceList);
            *ppInterfaceList = NULL;
        }

        size_t requiredSize = offsetof(WLAN_INTERFACE_INFO_LIST, InterfaceInfo) + sizeof(WLAN_INTERFACE_INFO);
        PWLAN_INTERFACE_INFO_LIST newList = (PWLAN_INTERFACE_INFO_LIST)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredSize);

        if (newList) {
            newList->dwNumberOfItems = 1;
            newList->dwIndex = 0; // Index for the first/only item

            WLAN_INTERFACE_INFO* pInfo = &(newList->InterfaceInfo[0]);
            memcpy(&(pInfo->InterfaceGuid), &currentSpoofedGuid, sizeof(GUID));
            WCHAR descriptionBuffer[256];
            swprintf_s(descriptionBuffer, _countof(descriptionBuffer), L"Spoofed Wi-Fi BSSID=%s", g_bssid);
            wcscpy_s(pInfo->strInterfaceDescription, _countof(pInfo->strInterfaceDescription), descriptionBuffer);
            pInfo->isState = (WLAN_INTERFACE_STATE)1; // wlan_interface_state_connected

            *ppInterfaceList = newList;
            // result remains ERROR_SUCCESS
        } else {
            // HeapAlloc failed for our new list
            *ppInterfaceList = NULL; // Ensure it's NULL
            result = ERROR_NOT_ENOUGH_MEMORY; // Update result to indicate failure
        }
    }
    // If result was not ERROR_SUCCESS initially, Real_WlanEnumInterfaces already set ppInterfaceList (likely to NULL)
    // and we return its error code.

    return result;
}

// Hook for WlanQueryInterface (revised to always return spoofed data for current connection)
DWORD WINAPI Hooked_WlanQueryInterface(HANDLE hClientHandle, const GUID *pInterfaceGuid, 
                                       WLAN_INTF_OPCODE OpCode, PVOID pReserved, 
                                       PDWORD pdwDataSize, PVOID *ppData, 
                                       PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType) {
    OutputDebugStringW(L"Hooked_WlanQueryInterface called");
    if (OpCode == wlan_intf_opcode_current_connection) {
        if (pInterfaceGuid == NULL) { // A GUID must be provided for this opcode as per MSDN.
            if (pdwDataSize) *pdwDataSize = 0;
            if (ppData) *ppData = NULL;
            return ERROR_INVALID_PARAMETER;
        }

        // If querying for current connection, always return our single spoofed interface's connection details,
        // regardless of the GUID passed (as long as one was passed).
        // This assumes WlanEnumInterfaces has already established there's only one (our spoofed) interface.

        DWORD structSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
        PWLAN_CONNECTION_ATTRIBUTES pConnAttr = (PWLAN_CONNECTION_ATTRIBUTES)WlanAllocateMemory(structSize);

        if (pConnAttr) {
            ZeroMemory(pConnAttr, structSize);

            pConnAttr->isState = wlan_interface_state_connected; 
            pConnAttr->wlanConnectionMode = wlan_connection_mode_profile;
            wcscpy_s(pConnAttr->strProfileName, _countof(pConnAttr->strProfileName), L"Spoofed WLAN Profile");

            const char* spoofedSsidName = "SpoofedSSID";
            pConnAttr->wlanAssociationAttributes.dot11Ssid.uSSIDLength = (ULONG)strlen(spoofedSsidName);
            memcpy(pConnAttr->wlanAssociationAttributes.dot11Ssid.ucSSID, spoofedSsidName, strlen(spoofedSsidName));
            pConnAttr->wlanAssociationAttributes.dot11BssType = dot11_BSS_type_infrastructure;
            
            BYTE spoofedBssid[6] = {0};
            if (!ParseMacString(g_bssid, spoofedBssid)) {
                spoofedBssid[0] = 0xDE; spoofedBssid[1] = 0xAD; spoofedBssid[2] = 0xBE;
                spoofedBssid[3] = 0xEF; spoofedBssid[4] = 0xCA; spoofedBssid[5] = 0xFE;
            }
            memcpy(pConnAttr->wlanAssociationAttributes.dot11Bssid, spoofedBssid, 6);
            
            pConnAttr->wlanAssociationAttributes.wlanSignalQuality = 100;
            pConnAttr->wlanAssociationAttributes.dot11PhyType = dot11_phy_type_vht; // Example: 802.11ac

            *pdwDataSize = structSize;
            *ppData = pConnAttr; 

            if (pWlanOpcodeValueType) {
                *pWlanOpcodeValueType = wlan_opcode_value_type_query_only;
            }
            return ERROR_SUCCESS;
        } else {
            if (pdwDataSize) *pdwDataSize = 0;
            if (ppData) *ppData = NULL;
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    // For all other opcodes, call the original function.
    return Real_WlanQueryInterface(hClientHandle, pInterfaceGuid, OpCode, pReserved,
                                   pdwDataSize, ppData, pWlanOpcodeValueType);
}

// Hook for WlanGetNetworkBssList to spoof BSS list
DWORD WINAPI Hooked_WlanGetNetworkBssList(HANDLE hClientHandle, const GUID *pInterfaceGuid,
                                         PDOT11_SSID pDot11Ssid, DOT11_BSS_TYPE dot11BssType,
                                         BOOL bSecurityEnabled, PVOID pReserved,
                                         PWLAN_BSS_LIST *ppWlanBssList) {
    OutputDebugStringW(L"Hooked_WlanGetNetworkBssList called");
    if (ppWlanBssList == NULL) {
        return ERROR_INVALID_PARAMETER;
    }
    *ppWlanBssList = NULL; // Initialize output parameter

    // If a specific interface GUID is provided, it must match our spoofed one.
    if (pInterfaceGuid != NULL) {
        GUID currentSpoofedGuid;
        char currentGuidStr[64];
        size_t convertedChars = 0;
        wcstombs_s(&convertedChars, currentGuidStr, sizeof(currentGuidStr), g_wlanGuid, _TRUNCATE);
        StringToGuid(currentGuidStr, &currentSpoofedGuid);

        if (memcmp(pInterfaceGuid, &currentSpoofedGuid, sizeof(GUID)) != 0) {
            return ERROR_INVALID_PARAMETER; // Or ERROR_NOT_FOUND for a non-matching GUID
        }
    }
    
    // Regardless of pDot11Ssid, dot11BssType, or bSecurityEnabled, return our single spoofed BSS entry.
    // These parameters would normally filter the list, but we only have one entry to give.

    size_t requiredSize = offsetof(WLAN_BSS_LIST, wlanBssEntries) + sizeof(WLAN_BSS_ENTRY);
    PWLAN_BSS_LIST newList = (PWLAN_BSS_LIST)WlanAllocateMemory(static_cast<DWORD>(requiredSize));

    if (newList == NULL) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    ZeroMemory(newList, requiredSize);

    newList->dwTotalSize = static_cast<DWORD>(requiredSize);
    newList->dwNumberOfItems = 1;

    WLAN_BSS_ENTRY* entry = &(newList->wlanBssEntries[0]);

    // SSID
    const char* spoofedSsidName = "SpoofedSSID";
    entry->dot11Ssid.uSSIDLength = static_cast<ULONG>(strlen(spoofedSsidName));
    memcpy(entry->dot11Ssid.ucSSID, spoofedSsidName, strlen(spoofedSsidName));

    // BSS Type
    entry->dot11BssType = dot11_BSS_type_infrastructure;

    // BSSID (MAC address of the AP)
    BYTE spoofedBssid[6];
    if (!ParseMacString(g_bssid, spoofedBssid)) {
        // Fallback BSSID
        spoofedBssid[0] = 0xAA; spoofedBssid[1] = 0xBB; spoofedBssid[2] = 0xCC;
        spoofedBssid[3] = 0xDD; spoofedBssid[4] = 0xEE; spoofedBssid[5] = 0xFF;
    }
    memcpy(entry->dot11Bssid, spoofedBssid, 6);

    entry->uLinkQuality = 100;        // Percentage (0-100)
    entry->lRssi = -30;                // dBm, strong signal
    entry->ulChCenterFrequency = 2412000; // Example: Channel 1 for 2.4GHz (in kHz)

    // dot11_phy_type_ht (802.11n) is 5. dot11_phy_type_vht (802.11ac) is 7.
    entry->uPhyId = 7; // Assuming 802.11ac (VHT)

    // Other potentially relevant fields for a more complete entry:
    entry->bInRegDomain = TRUE;         // Assume it's in the regulatory domain
    entry->usBeaconPeriod = 100;        // Typical beacon interval (ms)
    entry->ullTimestamp = GetTickCount64(); // Current system uptime as timestamp
    entry->ullHostTimestamp = GetTickCount64();
    entry->usCapabilityInformation = 0x0001; // ESS (bit 0 set)
    if (bSecurityEnabled) { // If the query was for secured networks, let's reflect that minimally.
        entry->usCapabilityInformation |= 0x0010; // Privacy (bit 4 set)
    }

    // Information Elements are handled by an offset and size within the BSS entry structure itself
    // when returned by the system. Since we are constructing it, and not adding IEs,
    // these should be zero.
    entry->ulIeOffset = 0;
    entry->ulIeSize = 0;

    *ppWlanBssList = newList; // Caller must free this with WlanFreeMemory
    return ERROR_SUCCESS;
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

// --- Hook Management Functions ---
bool InitializeNetworkHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourAttach(&(PVOID&)Real_GetAdaptersInfo, Hooked_GetAdaptersInfo);
    DetourAttach(&(PVOID&)Real_GetAdaptersAddresses, Hooked_GetAdaptersAddresses);
    DetourAttach(&(PVOID&)Real_WlanEnumInterfaces, Hooked_WlanEnumInterfaces);
    DetourAttach(&(PVOID&)Real_WlanQueryInterface, Hooked_WlanQueryInterface);
    DetourAttach(&(PVOID&)Real_WlanGetNetworkBssList, Hooked_WlanGetNetworkBssList);

    LONG error = DetourTransactionCommit();
    if (error != NO_ERROR) {
        // LogDLL(L"Failed to attach network hooks", HRESULT_FROM_WIN32(error)); // Assuming LogDLL is accessible or use OutputDebugStringW
        OutputDebugStringW(L"SPOOF_DLL: Failed to attach network hooks");
        return false;
    }
    return true;
}

void CleanupNetworkHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    DetourDetach(&(PVOID&)Real_GetAdaptersInfo, Hooked_GetAdaptersInfo);
    DetourDetach(&(PVOID&)Real_GetAdaptersAddresses, Hooked_GetAdaptersAddresses);
    DetourDetach(&(PVOID&)Real_WlanEnumInterfaces, Hooked_WlanEnumInterfaces);
    DetourDetach(&(PVOID&)Real_WlanQueryInterface, Hooked_WlanQueryInterface);
    DetourDetach(&(PVOID&)Real_WlanGetNetworkBssList, Hooked_WlanGetNetworkBssList);

    LONG error = DetourTransactionCommit();
    if (error != NO_ERROR) {
        // LogDLL(L"Failed to detach network hooks", HRESULT_FROM_WIN32(error));
        OutputDebugStringW(L"SPOOF_DLL: Failed to detach network hooks");
    }
    OutputDebugStringW(L"SPOOF_DLL: Network hooks detached successfully");
}