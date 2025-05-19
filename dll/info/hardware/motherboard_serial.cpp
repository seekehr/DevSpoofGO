#include <windows.h>
#include <cstring>    
#include <cstdio>     
#include "motherboard_serial.h"
#include <vector> // Required for std::vector

// --- Hardware information related global variables ---
WCHAR g_motherboardSerial[256] = L"F122RCX0031FBNC"; // Default value
static char g_originalMotherboardSerial[256] = "NOT_FETCHED";

// --- Function pointer to original Windows API function ---
static UINT (WINAPI *Real_GetSystemFirmwareTable_MB)(DWORD, DWORD, PVOID, DWORD) = nullptr;

// --- SMBIOS structure definitions ---
#pragma pack(push, 1)

typedef struct {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
} RawSMBIOSDataMB; // Renamed to avoid conflict if included elsewhere

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
    BYTE SerialNumber; // Index of string for baseboard serial number
    // Other fields not needed for this implementation
} BaseboardInfoMB;

#pragma pack(pop)

// --- Forward declarations ---
static const char* Internal_GetOriginalMotherboardSerial();
const char* ExtractSMBIOSStringMB(const BYTE* stringSection, BYTE index, const BYTE* pEnd);
const BYTE* FindNextSMBIOSStructureMB(const BYTE* p, const BYTE* pEnd);
bool FindBaseboardStructureMB(const BYTE* start, const BYTE* end, const BYTE** structStart, BYTE* serialIndex);

// --- Helper function to extract a string from SMBIOS data ---
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
            return ""; // Out of bounds
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

// --- Helper function to find next SMBIOS structure ---
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

// --- Helper function to find Type 2 structure ---
bool FindBaseboardStructureMB(const BYTE* start, const BYTE* end, 
                           const BYTE** structStart, BYTE* serialIndex) {
    const BYTE* p = start;
    while (p && p < end && (p + sizeof(SMBIOSStructHeaderMB) <= end)) {
        const SMBIOSStructHeaderMB* header = reinterpret_cast<const SMBIOSStructHeaderMB*>(p);
        if (header->Length < sizeof(SMBIOSStructHeaderMB)) return false; // Malformed

        if (header->Type == 2) { // Baseboard (Motherboard) Information
            const BaseboardInfoMB* bbInfo = reinterpret_cast<const BaseboardInfoMB*>(p);
            // Ensure structure is large enough for SerialNumber field
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

// --- Function to get the original motherboard serial ---
static const char* Internal_GetOriginalMotherboardSerial() {
    if (strcmp(g_originalMotherboardSerial, "NOT_FETCHED") != 0 && strcmp(g_originalMotherboardSerial, "") != 0) {
        return g_originalMotherboardSerial;
    }

    if (!Real_GetSystemFirmwareTable_MB) {
        strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "ERROR_NO_REAL_FUNC");
        return g_originalMotherboardSerial;
    }

    const DWORD signature = 'RSMB';
    const DWORD id = 0;
    
    UINT bufferSize = Real_GetSystemFirmwareTable_MB(signature, id, NULL, 0);
    if (bufferSize == 0) {
        strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "ERROR_BUFFER_SIZE");
        return g_originalMotherboardSerial;
    }
    
    std::vector<BYTE> buffer(bufferSize);
    
    UINT result = Real_GetSystemFirmwareTable_MB(signature, id, buffer.data(), bufferSize);
    if (result == 0) {
        strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "ERROR_FIRMWARE_TABLE");
        return g_originalMotherboardSerial;
    }
    
    RawSMBIOSDataMB* pSmbios = reinterpret_cast<RawSMBIOSDataMB*>(buffer.data());
    if (bufferSize < sizeof(RawSMBIOSDataMB) || result < sizeof(RawSMBIOSDataMB) || pSmbios->Length > bufferSize - offsetof(RawSMBIOSDataMB, SMBIOSTableData)) {
        strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "ERROR_INVALID_SMBIOS_HDR");
        return g_originalMotherboardSerial;
    }

    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    
    const BYTE* structStartPtr = nullptr;
    BYTE serialIdx = 0;
    
    if (FindBaseboardStructureMB(tableData, pEnd, &structStartPtr, &serialIdx)) {
        if (serialIdx == 0) {
            strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "NO_SERIAL_INDEX");
        } else {
            const BYTE* stringSection = structStartPtr + reinterpret_cast<const SMBIOSStructHeaderMB*>(structStartPtr)->Length;
            if (stringSection >= pEnd) {
                 strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "ERROR_STRING_SEC_OOB");
                 return g_originalMotherboardSerial;
            }
            const char* serial = ExtractSMBIOSStringMB(stringSection, serialIdx, pEnd);
            if (serial && serial[0] != '\0') {
                strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), serial);
            } else {
                strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "EMPTY_SERIAL");
            }
        }
    } else {
        strcpy_s(g_originalMotherboardSerial, sizeof(g_originalMotherboardSerial), "NO_BASEBOARD_FOUND");
    }
        
    return g_originalMotherboardSerial;
}

// --- Function for export ---
extern "C" {
    __declspec(dllexport) const char* GetOriginalMotherboardSerial() {
        return Internal_GetOriginalMotherboardSerial();
    }
}

// --- Function to set the spoofed motherboard serial ---
void SetSpoofedMotherboardSerial(const char* serial) {
    if (serial) {
        size_t converted = 0;
        mbstowcs_s(&converted, g_motherboardSerial, sizeof(g_motherboardSerial) / sizeof(WCHAR), serial, _TRUNCATE);
    } else {
        g_motherboardSerial[0] = L'\0';
    }
}

// --- Function to modify SMBIOS data for motherboard serial ---
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
                        currentStringPtr[spoofedSerialLen] = 0; 
                    }
                    OutputDebugStringW(L"MB_SPOOF: Motherboard Serial successfully spoofed.");
                } else {
                    OutputDebugStringW(L"MB_SPOOF: Spoofed Motherboard serial too long.");
                }
            } else {
                 OutputDebugStringW(L"MB_SPOOF: Original Motherboard serial length is 0.");
            }
        } else {
            OutputDebugStringW(L"MB_SPOOF: Serial index is 0 in SMBIOS Type 2.");
        }
    } else {
        OutputDebugStringW(L"MB_SPOOF: Baseboard (Type 2) structure not found for spoofing.");
    }
}


// --- Function to initialize and store the real function pointer ---
void InitializeMotherboardSerialHooks(PVOID realGetSystemFirmwareTable) {
    if (realGetSystemFirmwareTable) {
        Real_GetSystemFirmwareTable_MB = reinterpret_cast<UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD)>(realGetSystemFirmwareTable);
        OutputDebugStringW(L"MB_SPOOF: Real_GetSystemFirmwareTable_MB initialized.");
        Internal_GetOriginalMotherboardSerial(); // Fetch original serial
    } else {
         OutputDebugStringW(L"MB_SPOOF: Attempted to initialize with NULL Real_GetSystemFirmwareTable_MB.");
    }
}

// --- Function to get the spoofed motherboard serial for other modules ---
const WCHAR* GetSpoofedMotherboardSerial() {
    return g_motherboardSerial;
}
