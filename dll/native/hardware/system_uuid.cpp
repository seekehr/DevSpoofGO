#include "system_uuid.h"
#include <windows.h>
#include <rpc.h>
#include <string>
#include <vector>
#include <cstdio>

#pragma comment(lib, "Rpcrt4.lib")

GUID g_systemUuid = { 0x12345678, 0x1234, 0x5678, { 0x9A, 0xBC, 0xDE, 0xF0, 0x12, 0x34, 0x56, 0x78 } };

static UINT (WINAPI *Real_GetSystemFirmwareTable_SystemUuid)(DWORD, DWORD, PVOID, DWORD) = nullptr;

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

typedef struct {
    SMBIOSStructHeaderUuid Header;
    BYTE Manufacturer;
    BYTE ProductName;
    BYTE Version;
    BYTE SerialNumber;
    GUID UUID;
    BYTE WakeUpType;
    BYTE SKUNumber;
    BYTE Family;
} SystemInfoStructUuid;

#pragma pack(pop)

const BYTE* FindNextSMBIOSStructureUuid(const BYTE* p, const BYTE* pEnd);
bool FindSystemInfoStructureUuid(const BYTE* start, const BYTE* end, const BYTE** structStart);

const BYTE* FindNextSMBIOSStructureUuid(const BYTE* p, const BYTE* pEnd) {
    if (!p || p >= pEnd) return nullptr;
    const SMBIOSStructHeaderUuid* header = reinterpret_cast<const SMBIOSStructHeaderUuid*>(p);
    if (header->Length == 0) return nullptr;
    p += header->Length;
    while (p < pEnd - 1) {
        if (p[0] == 0 && p[1] == 0) return p + 2;
        p++;
    }
    return nullptr;
}

bool FindSystemInfoStructureUuid(const BYTE* start, const BYTE* end, const BYTE** structStart) {
    const BYTE* p = start;
    while (p && p < end && (p + sizeof(SMBIOSStructHeaderUuid) <= end)) {
        const SMBIOSStructHeaderUuid* header = reinterpret_cast<const SMBIOSStructHeaderUuid*>(p);
        if (header->Length < sizeof(SMBIOSStructHeaderUuid)) return false;
        if (header->Type == 1) {
            if (header->Length >= offsetof(SystemInfoStructUuid, UUID) + sizeof(GUID)) {
                *structStart = p;
                return true;
            }
        }
        p = FindNextSMBIOSStructureUuid(p, end);
    }
    return false;
}

void SetSpoofedSystemUuid(const char* uuidString) {
    if (uuidString && strlen(uuidString) > 0) {
        RPC_STATUS status = UuidFromStringA(reinterpret_cast<RPC_CSTR>(const_cast<char*>(uuidString)), &g_systemUuid);
        if (status != RPC_S_OK) {
            OutputDebugStringW(L"SYSTEM_UUID: Invalid UUID string format.");
        }
    }
}

void ModifySmbiosForSystemUuid(PVOID pFirmwareTableBuffer, DWORD BufferSize) {
    if (!pFirmwareTableBuffer || BufferSize < sizeof(RawSMBIOSDataUuid)) return;
    RawSMBIOSDataUuid* pSmbios = static_cast<RawSMBIOSDataUuid*>(pFirmwareTableBuffer);
    if (pSmbios->Length == 0 || pSmbios->Length > BufferSize - offsetof(RawSMBIOSDataUuid, SMBIOSTableData)) {
        OutputDebugStringW(L"SYSTEM_UUID: Invalid SMBIOS data length in ModifySmbiosForSystemUuid.");
        return;
    }
    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    const BYTE* structStart = nullptr;
    if (FindSystemInfoStructureUuid(tableData, pEnd, &structStart)) {
        SystemInfoStructUuid* sysInfo = reinterpret_cast<SystemInfoStructUuid*>(const_cast<BYTE*>(structStart));
        if ( (const BYTE*)&(sysInfo->UUID) + sizeof(GUID) <= pEnd ) {
            sysInfo->UUID = g_systemUuid;
        } else {
            OutputDebugStringW(L"SYSTEM_UUID: Error: UUID field extends beyond SMBIOS data boundary.");
        }
    } else {
        OutputDebugStringW(L"SYSTEM_UUID: System Information (Type 1) SMBIOS structure not found or too small for UUID.");
    }
    OutputDebugStringW(L"SYSTEM_UUID: ModifySmbiosForSystemUuid called.");
}

PVOID* GetRealGetSystemFirmwareTableForUuid() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_SystemUuid);
}

void InitializeSystemUuidHooks(PVOID realGetSystemFirmwareTable) {
    if (realGetSystemFirmwareTable) {
        Real_GetSystemFirmwareTable_SystemUuid = reinterpret_cast<UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD)>(realGetSystemFirmwareTable);
    } else {
        OutputDebugStringW(L"SYSTEM_UUID: Init with NULL Real_GetSystemFirmwareTable pointer for UUID module!");
    }
}
