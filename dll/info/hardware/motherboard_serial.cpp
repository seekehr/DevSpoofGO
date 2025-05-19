#include <windows.h>
#include <cstring>
#include <cstdio>
#include "motherboard_serial.h"
#include <vector>

WCHAR g_motherboardSerial[256] = L"F122RCX0031FBNC";
static char g_originalMotherboardSerial[256] = "NOT_FETCHED";

static UINT (WINAPI *Real_GetSystemFirmwareTable_MB)(DWORD, DWORD, PVOID, DWORD) = nullptr;

#pragma pack(push, 1)

typedef struct {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
} RawSMBIOSDataMB;

typedef struct {
    BYTE Type;
    BYTE Length;
    WORD Handle;
} SMBIOSStructHeaderMB;

typedef struct {
    SMBIOSStructHeaderMB Header;
    BYTE Manufacturer;
    BYTE Product;
    BYTE Version;
    BYTE SerialNumber;
} BaseboardInfoMB;

#pragma pack(pop)

const char* ExtractSMBIOSStringMB(const BYTE* stringSection, BYTE index, const BYTE* pEnd);
const BYTE* FindNextSMBIOSStructureMB(const BYTE* p, const BYTE* pEnd);
bool FindBaseboardStructureMB(const BYTE* start, const BYTE* end, const BYTE** structStart, BYTE* serialIndex);

const char* ExtractSMBIOSStringMB(const BYTE* stringSection, BYTE index, const BYTE* pEnd) {
    static char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    if (index == 0) return "";
    const BYTE* strPtr = stringSection;
    for (BYTE i = 1; i < index; i++) {
        while (strPtr < pEnd && *strPtr != 0) {
            strPtr++;
        }
        if (strPtr < pEnd) {
            strPtr++;
        } else {
            return "";
        }
    }
    size_t len = 0;
    const BYTE* tempPtr = strPtr;
    while (tempPtr < pEnd && *tempPtr != 0 && len < (sizeof(buffer) - 1)) {
        buffer[len++] = static_cast<char>(*tempPtr);
        tempPtr++;
    }
    buffer[len] = '\0';
    return buffer;
}

const BYTE* FindNextSMBIOSStructureMB(const BYTE* p, const BYTE* pEnd) {
    if (!p || p >= pEnd) return nullptr;
    const SMBIOSStructHeaderMB* header = reinterpret_cast<const SMBIOSStructHeaderMB*>(p);
    p += header->Length;
    while (p < pEnd - 1) {
        if (p[0] == 0 && p[1] == 0) {
            return p + 2;
        }
        p++;
    }
    return nullptr;
}

bool FindBaseboardStructureMB(const BYTE* start, const BYTE* end,
                           const BYTE** structStart, BYTE* serialIndex) {
    const BYTE* p = start;
    while (p && p < end && (p + sizeof(SMBIOSStructHeaderMB) <= end)) {
        const SMBIOSStructHeaderMB* header = reinterpret_cast<const SMBIOSStructHeaderMB*>(p);
        if (header->Length < sizeof(SMBIOSStructHeaderMB)) return false;
        if (header->Type == 2) {
            const BaseboardInfoMB* bbInfo = reinterpret_cast<const BaseboardInfoMB*>(p);
            if (header->Length >= offsetof(BaseboardInfoMB, SerialNumber) + sizeof(bbInfo->SerialNumber)) {
                 *structStart = p;
                 *serialIndex = bbInfo->SerialNumber;
                 return true;
            }
        }
        p = FindNextSMBIOSStructureMB(p, end);
    }
    return false;
}

void SetSpoofedMotherboardSerial(const char* serial) {
    if (serial) {
        size_t converted = 0;
        mbstowcs_s(&converted, g_motherboardSerial, sizeof(g_motherboardSerial) / sizeof(WCHAR), serial, _TRUNCATE);
    } else {
        g_motherboardSerial[0] = L'\0';
    }
}

void ModifySmbiosForMotherboardSerial(PVOID pFirmwareTableBuffer, DWORD BufferSize) {
    if (!pFirmwareTableBuffer || BufferSize < sizeof(RawSMBIOSDataMB)) {
        OutputDebugStringW(L"MB_SPOOF: Invalid buffer or size for modification.");
        return;
    }
    RawSMBIOSDataMB* pSmbios = static_cast<RawSMBIOSDataMB*>(pFirmwareTableBuffer);
    if (pSmbios->Length > BufferSize - offsetof(RawSMBIOSDataMB, SMBIOSTableData)) {
         OutputDebugStringW(L"MB_SPOOF: SMBIOS length exceeds buffer capacity.");
         return;
    }
    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    const BYTE* structStartPtr = nullptr;
    BYTE serialIdx = 0;
    if (FindBaseboardStructureMB(tableData, pEnd, &structStartPtr, &serialIdx)) {
        if (serialIdx != 0) {
            const BYTE* stringSectionStart = structStartPtr +
                reinterpret_cast<const SMBIOSStructHeaderMB*>(structStartPtr)->Length;
            if (stringSectionStart >= pEnd) {
                 OutputDebugStringW(L"MB_SPOOF: String section out of bounds.");
                 return;
            }
            BYTE* currentStringPtr = const_cast<BYTE*>(stringSectionStart);
            for (BYTE i = 1; i < serialIdx; ++i) {
                while (currentStringPtr < pEnd && *currentStringPtr != 0) {
                    currentStringPtr++;
                }
                if (currentStringPtr < pEnd) {
                    currentStringPtr++;
                } else {
                    OutputDebugStringW(L"MB_SPOOF: Error locating Nth serial string.");
                    return;
                }
            }
            if (currentStringPtr >= pEnd) {
                 OutputDebugStringW(L"MB_SPOOF: Serial string pointer out of bounds before spoofing.");
                 return;
            }
            size_t originalSerialLen = 0;
            const BYTE* tempPtr = currentStringPtr;
            while (tempPtr < pEnd && *tempPtr != 0) {
                originalSerialLen++;
                tempPtr++;
            }
            if (originalSerialLen > 0) {
                char spoofedSerialA[256];
                size_t convertedChars = 0;
                wcstombs_s(&convertedChars, spoofedSerialA, sizeof(spoofedSerialA), g_motherboardSerial, _TRUNCATE);
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

void InitializeMotherboardSerialHooks(PVOID realGetSystemFirmwareTable) {
    if (realGetSystemFirmwareTable) {
        Real_GetSystemFirmwareTable_MB = reinterpret_cast<UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD)>(realGetSystemFirmwareTable);
    } else {
        OutputDebugStringW(L"MB_SPOOF: Init with NULL Real_GetSystemFirmwareTable_MB pointer!");
    }
}
