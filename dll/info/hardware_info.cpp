#include <windows.h>
#include <cstring>    // For strlen, wcscpy_s, mbstowcs_s
#include <cstdio>     // For sprintf_s
#include "hardware_info.h"

// --- Hardware information related global variables ---
// Store the spoofed motherboard serial number
WCHAR g_motherboardSerial[256] = L"F122RCX0031FBNC"; // Default valu

// --- Function pointer to original Windows API function ---
// Pointer to the original GetSystemFirmwareTable function
static UINT (WINAPI *Real_GetSystemFirmwareTable)(DWORD, DWORD, PVOID, DWORD) = GetSystemFirmwareTable;

// --- SMBIOS structure definitions ---
#pragma pack(push, 1)

// SMBIOS header structure
typedef struct {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
} RawSMBIOSData;

// SMBIOS structure header
typedef struct {
    BYTE Type;
    BYTE Length;
    WORD Handle;
} SMBIOSStructHeader;

// SMBIOS Type 2 (Baseboard Information) structure
typedef struct {
    SMBIOSStructHeader Header;
    BYTE Manufacturer;
    BYTE Product;
    BYTE Version;
    BYTE SerialNumber;
    // Other fields not needed for this implementation
} BaseboardInfo;

#pragma pack(pop)

// --- Forward declarations ---
static const char* Internal_GetMotherboardSerial();

// --- Helper function to extract a string from SMBIOS data ---
// Extracts a string at the given index from the string section
const char* ExtractSMBIOSString(const BYTE* stringSection, BYTE index, const BYTE* pEnd) {
    static char buffer[256] = {0};
    
    if (index == 0) return ""; // No string
    
    // Find the Nth string (index)
    const BYTE* strPtr = stringSection;
    for (BYTE i = 1; i < index; i++) {
        // Skip to next string
        while (strPtr < pEnd && *strPtr != 0) {
            strPtr++;
        }
        // Skip past null terminator
        strPtr++;
        
        if (strPtr >= pEnd) {
            return ""; // Out of bounds
        }
    }
    
    // Calculate string length (up to null or end of buffer)
    size_t maxLen = min(sizeof(buffer)-1, static_cast<size_t>(pEnd - strPtr));
    size_t len = 0;
    while (len < maxLen && strPtr[len] != 0) {
        len++;
    }
    
    // Copy the string to our buffer
    if (len > 0) {
        memcpy(buffer, strPtr, len);
        buffer[len] = 0; // Ensure null termination
        return buffer;
    }
    
    return ""; // Empty string
}

// --- Helper function to find next SMBIOS structure ---
// Returns pointer to next structure or NULL if none found
const BYTE* FindNextSMBIOSStructure(const BYTE* p, const BYTE* pEnd) {
    // Skip the fixed part
    p += reinterpret_cast<const SMBIOSStructHeader*>(p)->Length;
    
    // Make sure we're still in bounds
    if (p >= pEnd - 1) return nullptr;
    
    // Skip the string-set (find double NULL)
    while (p < pEnd - 1) {
        if (p[0] == 0 && p[1] == 0) {
            // Found double null, move past it
            return p + 2;
        }
        p++;
    }
    
    return nullptr; // No more structures
}

// --- Helper function to find Type 2 structure ---
bool FindBaseboardStructure(const BYTE* start, const BYTE* end, 
                           const BYTE** structStart, BYTE* serialIndex) {
    const BYTE* p = start;
    
    while (p < end - 4) { // Need at least header size
        const SMBIOSStructHeader* header = reinterpret_cast<const SMBIOSStructHeader*>(p);
        
        // Check if this is Type 2 (Baseboard)
        if (header->Type == 2) {
            // Make sure we have enough data for serial index
            if (p + 0x07 < end) {
                *structStart = p;
                *serialIndex = *(p + 0x07); // Serial number index
                return true;
            }
            break;
        }
        
        // Move to next structure
        p = FindNextSMBIOSStructure(p, end);
        if (!p) break;
    }
    
    return false;
}

// --- Function to get the original motherboard serial ---
static const char* Internal_GetMotherboardSerial() {
    OutputDebugStringW(L"Internal_GetMotherboardSerial called");
    static char serialBuffer[256] = {0};
    
    const DWORD signature = 'RSMB'; // SMBIOS
    const DWORD id = 0;             // Only one table for SMBIOS
    
    // First get the required buffer size
    UINT bufferSize = Real_GetSystemFirmwareTable(signature, id, NULL, 0);
    if (bufferSize == 0) {
        return "ERROR_BUFFER_SIZE";
    }
    
    // Allocate memory for the buffer
    BYTE* buffer = new BYTE[bufferSize];
    if (!buffer) {
        return "ERROR_MEMORY";
    }
    
    // Get the actual SMBIOS data
    UINT result = Real_GetSystemFirmwareTable(signature, id, buffer, bufferSize);
    if (result == 0) {
        delete[] buffer;
        return "ERROR_FIRMWARE_TABLE";
    }
    
    // Process SMBIOS data
    RawSMBIOSData* pSmbios = reinterpret_cast<RawSMBIOSData*>(buffer);
    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    
    // Find the baseboard structure
    const BYTE* structStart = nullptr;
    BYTE serialIndex = 0;
    
    if (FindBaseboardStructure(tableData, pEnd, &structStart, &serialIndex)) {
        if (serialIndex == 0) {
            strcpy_s(serialBuffer, sizeof(serialBuffer), "NO_SERIAL_INDEX");
        } else {
            // Find the string section
            const BYTE* stringSection = structStart + 
                reinterpret_cast<const SMBIOSStructHeader*>(structStart)->Length;
            
            // Extract the serial number string
            const char* serial = ExtractSMBIOSString(stringSection, serialIndex, pEnd);
            if (serial && serial[0] != '\0') {
                strcpy_s(serialBuffer, sizeof(serialBuffer), serial);
            } else {
                strcpy_s(serialBuffer, sizeof(serialBuffer), "EMPTY_SERIAL");
            }
        }
    } else {
        strcpy_s(serialBuffer, sizeof(serialBuffer), "NO_BASEBOARD_FOUND");
    }
    
    // Clean up
    delete[] buffer;
    
    return serialBuffer;
}

// --- Function for export ---
extern "C" {
    __declspec(dllexport) const char* GetOriginalMotherboardSerial() {
        return Internal_GetMotherboardSerial();
    }
}

// --- Hook function for GetSystemFirmwareTable ---
UINT WINAPI Hooked_GetSystemFirmwareTable(DWORD FirmwareTableProviderSignature, 
                                           DWORD FirmwareTableID, 
                                           PVOID pFirmwareTableBuffer, 
                                           DWORD BufferSize) {
    // Debug output
    OutputDebugStringW(L"Hooked_GetSystemFirmwareTable called");
    
    // First, call the original function to get the actual data
    UINT result = Real_GetSystemFirmwareTable(FirmwareTableProviderSignature, 
                                             FirmwareTableID, 
                                             pFirmwareTableBuffer, 
                                             BufferSize);
    
    // Debug output the results
    WCHAR debugMsg[256];
    swprintf_s(debugMsg, _countof(debugMsg), L"GetSystemFirmwareTable returned %u bytes, signature: %08X", result, FirmwareTableProviderSignature);
    OutputDebugStringW(debugMsg);
    
    // Only modify the data if:
    // 1. The call was successful
    // 2. We're looking at SMBIOS data ('RSMB')
    // 3. We have a buffer to work with
    // 4. The buffer size is sufficient
    if (result > 0 && 
        FirmwareTableProviderSignature == 'RSMB' && 
        pFirmwareTableBuffer != nullptr && 
        BufferSize >= sizeof(RawSMBIOSData)) {
        
        OutputDebugStringW(L"Processing SMBIOS data for spoofing");
        
        RawSMBIOSData* pSmbios = static_cast<RawSMBIOSData*>(pFirmwareTableBuffer);
        const BYTE* tableData = pSmbios->SMBIOSTableData;
        const BYTE* pEnd = tableData + pSmbios->Length;
        
        // Find the baseboard structure
        const BYTE* structStart = nullptr;
        BYTE serialIndex = 0;
        
        if (FindBaseboardStructure(tableData, pEnd, &structStart, &serialIndex)) {
            swprintf_s(debugMsg, _countof(debugMsg), L"Found baseboard structure, serial index: %d", serialIndex);
            OutputDebugStringW(debugMsg);
            
            if (serialIndex != 0) {
                // Find the string section
                const BYTE* stringSection = structStart + 
                    reinterpret_cast<const SMBIOSStructHeader*>(structStart)->Length;
                
                // Find the string with the given index
                const BYTE* strPtr = stringSection;
                bool found = false;
                
                for (BYTE i = 1; i < serialIndex; i++) {
                    // Skip to next string
                    while (strPtr < pEnd && *strPtr != 0) {
                        strPtr++;
                    }
                    // Skip past null terminator
                    strPtr++;
                    
                    if (strPtr >= pEnd) {
                        found = true;
                        break;
                    }
                }
                
                // If we found the right string position and it's within bounds
                if (!found && strPtr < pEnd) {
                    // Get the original serial string
                    char originalSerial[256] = {0};
                    size_t originalSerialLen = 0;
                    const BYTE* tempPtr = strPtr;
                    while (tempPtr < pEnd && *tempPtr != 0) {
                        if (originalSerialLen < sizeof(originalSerial) - 1) {
                            originalSerial[originalSerialLen++] = *tempPtr;
                        }
                        tempPtr++;
                    }
                    
                    swprintf_s(debugMsg, _countof(debugMsg), L"Original serial: '%hs', length: %zu", originalSerial, originalSerialLen);
                    OutputDebugStringW(debugMsg);
                    
                    // Only proceed if we have a valid serial string
                    if (originalSerialLen > 0) {
                        // Convert our spoofed serial to ANSI for modification
                        char spoofedSerialA[256] = {0};
                        size_t convertedChars = 0;
                        wcstombs_s(&convertedChars, spoofedSerialA, sizeof(spoofedSerialA), 
                                  g_motherboardSerial, _TRUNCATE);
                        
                        swprintf_s(debugMsg, _countof(debugMsg), L"Original serial: '%hs', length: %zu", originalSerial, originalSerialLen);
                        OutputDebugStringW(debugMsg);
                        
                        // Get the length of our spoofed serial
                        size_t spoofedSerialLen = strlen(spoofedSerialA);
                        
                        // Only overwrite if the spoofed serial will fit in the same space
                        if (spoofedSerialLen <= originalSerialLen) {
                            // Copy our spoofed serial over the original
                            memcpy(const_cast<BYTE*>(strPtr), spoofedSerialA, spoofedSerialLen);
                            
                            // If our serial is shorter, null-terminate it properly
                            if (spoofedSerialLen < originalSerialLen) {
                                const_cast<BYTE*>(strPtr)[spoofedSerialLen] = 0;
                            }
                            
                            OutputDebugStringW(L"Successfully spoofed the motherboard serial");
                        } else {
                            swprintf_s(debugMsg, _countof(debugMsg), L"Spoofed serial too long (%zu > %zu)", spoofedSerialLen, originalSerialLen);
                            OutputDebugStringW(debugMsg);
                        }
                    } else {
                        OutputDebugStringW(L"Original serial length is 0, can't spoof");
                    }
                } else {
                    OutputDebugStringW(L"Failed to find the serial string in the SMBIOS data");
                }
            } else {
                OutputDebugStringW(L"Serial index is 0, can't locate the serial string");
            }
        } else {
            OutputDebugStringW(L"Could not find baseboard structure in SMBIOS data");
        }
    } else {
        if (result == 0) {
            OutputDebugStringW(L"GetSystemFirmwareTable returned 0 bytes");
        } else if (FirmwareTableProviderSignature != 'RSMB') {
            swprintf_s(debugMsg, L"Not SMBIOS data (signature: %08X)", FirmwareTableProviderSignature);
            OutputDebugStringW(debugMsg);
        } else if (pFirmwareTableBuffer == nullptr) {
            OutputDebugStringW(L"NULL buffer provided");
        } else if (BufferSize < sizeof(RawSMBIOSData)) {
            swprintf_s(debugMsg, _countof(debugMsg), L"Buffer too small (%u < %zu)", BufferSize, sizeof(RawSMBIOSData));
            OutputDebugStringW(debugMsg);
        }
    }
    
    return result;
}

// --- Function to set the spoofed motherboard serial ---
void SetSpoofedMotherboardSerial(const char* serial) {
    if (serial) {
        // Convert the input ANSI string (char*) to a wide character string (WCHAR*)
        const size_t len = strlen(serial) + 1; // Include space for null terminator
        size_t converted = 0;
        // Use mbstowcs_s for secure conversion to WCHAR
        mbstowcs_s(&converted, g_motherboardSerial, sizeof(g_motherboardSerial) / sizeof(WCHAR), 
                  serial, len - 1);
        // mbstowcs_s automatically null-terminates if the source fits.
    }
}

// --- Helper functions for hooking ---
PVOID* GetRealGetSystemFirmwareTable() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable);
}

PVOID GetHookedGetSystemFirmwareTable() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable);
}

// --- Function to get the spoofed motherboard serial for other modules ---
const WCHAR* GetSpoofedMotherboardSerial() {
    return g_motherboardSerial;
}
