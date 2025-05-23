#include "bios_serial.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <vector>

WCHAR g_biosSerial[256] = L"BIOSSERIAL12345";

static UINT (WINAPI *Real_GetSystemFirmwareTable_Bios)(DWORD, DWORD, PVOID, DWORD) = nullptr;

#pragma pack(push, 1)

typedef struct {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
} RawSMBIOSDataBios;

typedef struct {
    BYTE Type;
    BYTE Length;
    WORD Handle;
} SMBIOSStructHeaderBios;

typedef struct {
    SMBIOSStructHeaderBios Header;
    BYTE Manufacturer;
    BYTE ProductName;
    BYTE Version;
    BYTE SerialNumber;
    GUID UUID;
    BYTE WakeUpType;
    BYTE SKUNumber;
    BYTE Family;
} SystemInfoStruct;

#pragma pack(pop)

const char* ExtractSMBIOSStringBios(const BYTE* stringSection, BYTE index, const BYTE* pEnd);
const BYTE* FindNextSMBIOSStructureBios(const BYTE* p, const BYTE* pEnd);
bool FindSystemInfoStructure(const BYTE* start, const BYTE* end, const BYTE** structStart, BYTE* serialIndex);

const char* ExtractSMBIOSStringBios(const BYTE* stringSection, BYTE index, const BYTE* pEnd) {
    static char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    if (index == 0) return "";
    const BYTE* strPtr = stringSection;
    for (BYTE i = 1; i < index; ++i) {
        while (strPtr < pEnd && *strPtr != 0) strPtr++;
        if (strPtr < pEnd) strPtr++;
        else return "";
    }
    size_t len = 0;
    const BYTE* tempPtr = strPtr;
    while (tempPtr < pEnd && *tempPtr != 0 && len < (sizeof(buffer) - 1)) {
        buffer[len++] = static_cast<char>(*tempPtr++);
    }
    buffer[len] = '\0';
    return buffer;
}

const BYTE* FindNextSMBIOSStructureBios(const BYTE* p, const BYTE* pEnd) {
    if (!p || p >= pEnd) return nullptr;
    const SMBIOSStructHeaderBios* header = reinterpret_cast<const SMBIOSStructHeaderBios*>(p);
    if (header->Length == 0) return nullptr;
    p += header->Length;
    while (p < pEnd -1) {
        if (p[0] == 0 && p[1] == 0) return p + 2;
        p++;
    }
    return nullptr;
}

bool FindSystemInfoStructure(const BYTE* start, const BYTE* end,
                           const BYTE** structStart, BYTE* serialIndex) {
    const BYTE* p = start;
    while (p && p < end && (p + sizeof(SMBIOSStructHeaderBios) <= end) ) {
        const SMBIOSStructHeaderBios* header = reinterpret_cast<const SMBIOSStructHeaderBios*>(p);
        if (header->Length < sizeof(SMBIOSStructHeaderBios)) return false;
        if (header->Type == 1) {
            const SystemInfoStruct* sysInfo = reinterpret_cast<const SystemInfoStruct*>(p);
            if (header->Length >= offsetof(SystemInfoStruct, SerialNumber) + sizeof(sysInfo->SerialNumber)) {
                 *structStart = p;
                 *serialIndex = sysInfo->SerialNumber;
                 return true;
            }
        }
        p = FindNextSMBIOSStructureBios(p, end);
    }
    return false;
}

void SetSpoofedBiosSerial(const char* serial) {
    if (serial) {
        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, g_biosSerial, sizeof(g_biosSerial) / sizeof(WCHAR), serial, _TRUNCATE);
    } else {
        g_biosSerial[0] = L'\0';
    }
}

const WCHAR* GetSpoofedBiosSerialNumber() {
    return g_biosSerial;
}

void ModifySmbiosForBiosSerial(PVOID pFirmwareTableBuffer, DWORD BufferSize) {
    if (!pFirmwareTableBuffer || BufferSize < sizeof(RawSMBIOSDataBios)) return;
    RawSMBIOSDataBios* pSmbios = static_cast<RawSMBIOSDataBios*>(pFirmwareTableBuffer);
    if (pSmbios->Length == 0 || pSmbios->Length > BufferSize - offsetof(RawSMBIOSDataBios, SMBIOSTableData)) return;
    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    const BYTE* structStart = nullptr;
    BYTE serialIdx = 0;
    if (FindSystemInfoStructure(tableData, pEnd, &structStart, &serialIdx)) {
        if (serialIdx != 0) {
            const SMBIOSStructHeaderBios* header = reinterpret_cast<const SMBIOSStructHeaderBios*>(structStart);
            if (structStart + header->Length > pEnd) return;
            const BYTE* stringSectionStart = structStart + header->Length;
            BYTE* currentStringPtr = const_cast<BYTE*>(stringSectionStart);
            for (BYTE i = 1; i < serialIdx; ++i) {
                while (currentStringPtr < pEnd && *currentStringPtr != 0) currentStringPtr++;
                if (currentStringPtr < pEnd) currentStringPtr++;
                else return;
            }
            if (currentStringPtr >= pEnd) return;
            size_t originalSerialLen = 0;
            const BYTE* tempPtr = currentStringPtr;
            while (tempPtr < pEnd && *tempPtr != 0) {
                originalSerialLen++;
                tempPtr++;
            }
            if (originalSerialLen > 0) {
                char spoofedSerialA[256];
                size_t convertedChars = 0;
                wcstombs_s(&convertedChars, spoofedSerialA, sizeof(spoofedSerialA), g_biosSerial, _TRUNCATE);
                size_t spoofedSerialLen = strlen(spoofedSerialA);
                if (spoofedSerialLen <= originalSerialLen) {
                    memcpy(currentStringPtr, spoofedSerialA, spoofedSerialLen);
                    if (spoofedSerialLen < originalSerialLen) {
                        currentStringPtr[spoofedSerialLen] = '\0';
                    }
                }
            }
        }
    }
}

PVOID* GetRealGetSystemFirmwareTableForBios() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Bios);
}

void InitializeBiosSerialHooks(PVOID realGetSystemFirmwareTable) {
    if (realGetSystemFirmwareTable) {
        Real_GetSystemFirmwareTable_Bios = reinterpret_cast<UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD)>(realGetSystemFirmwareTable);
    } else {
        OutputDebugStringW(L"BIOS_SPOOF: CRITICAL - Init with NULL Real_GetSystemFirmwareTable_Bios pointer!");
    }
}
