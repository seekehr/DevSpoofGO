#include "system_uuid.h"
#include <windows.h>
#include <rpc.h> // For UuidFromStringA
#include <string>
#include <vector>
#include <cstdio> // For OutputDebugStringW and swprintf_s if needed

#pragma comment(lib, "Rpcrt4.lib") // Link against Rpcrt4.lib for UuidFromStringA

// --- Global variable for the spoofed System UUID ---
GUID g_systemUuid = { 0x12345678, 0x1234, 0x5678, { 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78 } }; // Default UUID

// --- Function pointer to the original GetSystemFirmwareTable function ---
static UINT (WINAPI *Real_GetSystemFirmwareTable_SystemUuid)(DWORD, DWORD, PVOID, DWORD) = nullptr;

// --- SMBIOS structure definitions (similar to bios_serial.cpp and motherboard_serial.cpp) ---
#pragma pack(push, 1)

typedef struct {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
} RawSMBIOSDataUuid;

typedef struct {
    BYTE Type;
    BYTE Length;
    WORD Handle;
} SMBIOSStructHeaderUuid;

// System Information (Type 1)
typedef struct {
    SMBIOSStructHeaderUuid Header;
    BYTE Manufacturer;       // Offset 0x04
    BYTE ProductName;        // Offset 0x05
    BYTE Version;            // Offset 0x06
    BYTE SerialNumber;       // Offset 0x07
    GUID UUID;               // Offset 0x08
    BYTE WakeUpType;         // Offset 0x18
    BYTE SKUNumber;          // Offset 0x19
    BYTE Family;             // Offset 0x1A
} SystemInfoStructUuid;

#pragma pack(pop)

// --- Forward declarations for helper functions ---
const BYTE* FindNextSMBIOSStructureUuid(const BYTE* p, const BYTE* pEnd);
bool FindSystemInfoStructureUuid(const BYTE* start, const BYTE* end, const BYTE** structStart);

// --- Helper function to find the next SMBIOS structure ---
const BYTE* FindNextSMBIOSStructureUuid(const BYTE* p, const BYTE* pEnd) {
    if (!p || p >= pEnd) return nullptr;
    const SMBIOSStructHeaderUuid* header = reinterpret_cast<const SMBIOSStructHeaderUuid*>(p);
    if (header->Length == 0) return nullptr; // Avoid infinite loop on malformed entry
    p += header->Length;
    while (p < pEnd - 1) {
        if (p[0] == 0 && p[1] == 0) return p + 2; // Skip double null terminator
        p++;
    }
    return nullptr;
}

// --- Helper function to find Type 1 (System Information) structure ---
bool FindSystemInfoStructureUuid(const BYTE* start, const BYTE* end, const BYTE** structStart) {
    const BYTE* p = start;
    while (p && p < end && (p + sizeof(SMBIOSStructHeaderUuid) <= end)) {
        const SMBIOSStructHeaderUuid* header = reinterpret_cast<const SMBIOSStructHeaderUuid*>(p);
        if (header->Length < sizeof(SMBIOSStructHeaderUuid)) return false;

        if (header->Type == 1) { // System Information structure
            // Check if the structure is large enough to contain the UUID field
            if (header->Length >= offsetof(SystemInfoStructUuid, UUID) + sizeof(GUID)) {
                *structStart = p;
                return true;
            }
        }
        p = FindNextSMBIOSStructureUuid(p, end);
    }
    return false;
}

// --- Function to set the spoofed System UUID ---
void SetSpoofedSystemUuid(const char* uuidString) {
    if (uuidString && strlen(uuidString) > 0) {
        RPC_STATUS status = UuidFromStringA(reinterpret_cast<RPC_CSTR>(const_cast<char*>(uuidString)), &g_systemUuid);
        if (status != RPC_S_OK) {
            OutputDebugStringW(L"SYSTEM_UUID: Invalid UUID string format.");
        }
    } 
}

// --- Function to get the spoofed System UUID ---
const GUID& GetSpoofedSystemUuid() {
    return g_systemUuid;
}

// --- Function to modify SMBIOS data for System UUID ---
void ModifySmbiosForSystemUuid(PVOID pFirmwareTableBuffer, DWORD BufferSize) {
    if (!pFirmwareTableBuffer || BufferSize < sizeof(RawSMBIOSDataUuid)) return;

    RawSMBIOSDataUuid* pSmbios = static_cast<RawSMBIOSDataUuid*>(pFirmwareTableBuffer);
    // Check if the SMBIOS data length is valid
    if (pSmbios->Length == 0 || pSmbios->Length > BufferSize - offsetof(RawSMBIOSDataUuid, SMBIOSTableData)) {
        OutputDebugStringW(L"SYSTEM_UUID: Invalid SMBIOS data length in ModifySmbiosForSystemUuid.");
        return;
    }

    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    const BYTE* structStart = nullptr;

    if (FindSystemInfoStructureUuid(tableData, pEnd, &structStart)) {
        SystemInfoStructUuid* sysInfo = reinterpret_cast<SystemInfoStructUuid*>(const_cast<BYTE*>(structStart));
        
        // Directly overwrite the UUID field
        if ( (const BYTE*)&(sysInfo->UUID) + sizeof(GUID) <= pEnd ) {
            sysInfo->UUID = g_systemUuid;
        } else {
            OutputDebugStringW(L"SYSTEM_UUID: Error: UUID field extends beyond SMBIOS data boundary.");
        }
    } else {
        OutputDebugStringW(L"SYSTEM_UUID: System Information (Type 1) SMBIOS structure not found or too small for UUID.");
    }
}

// --- Helper functions for hooking (to be managed by hardware_info or a central hook manager) ---
PVOID* GetRealGetSystemFirmwareTableForUuid() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_SystemUuid);
}

void InitializeSystemUuidHooks(PVOID realGetSystemFirmwareTable) {
    if (realGetSystemFirmwareTable) {
        Real_GetSystemFirmwareTable_SystemUuid = reinterpret_cast<UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD)>(realGetSystemFirmwareTable);
        OutputDebugStringW(L"SYSTEM_UUID: Real_GetSystemFirmwareTable initialized for UUID module.");
    } else {
        OutputDebugStringW(L"SYSTEM_UUID: Init with NULL Real_GetSystemFirmwareTable pointer for UUID module!");
    }
}
