#include <windows.h>
#include <cstring>    // For strlen, wcscpy_s, mbstowcs_s
#include "hardware_info.h"

// --- Hardware information related global variables ---
// Store the spoofed motherboard serial number
WCHAR g_motherboardSerial[256] = L"SPOOFED-SERIAL-DEFAULT"; // Default value

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

// --- Helper functions ---

// Finds the Type 2 (Baseboard) structure in the SMBIOS data and returns the serial number
const char* ExtractMotherboardSerial(const RawSMBIOSData* pData) {
    if (!pData) return nullptr;
    
    const BYTE* pEnd = pData->SMBIOSTableData + pData->Length;
    const BYTE* p = pData->SMBIOSTableData;
    
    // Iterate through structures
    while (p < pEnd) {
        const SMBIOSStructHeader* header = reinterpret_cast<const SMBIOSStructHeader*>(p);
        
        // Check for valid header and sufficient space
        if (header->Type == 0 && header->Length == 0) break;
        if (p + header->Length >= pEnd) break;
        
        // Check if this is a Baseboard (Type 2) structure
        if (header->Type == 2 && header->Length >= 8) { // 8 = offset to SerialNumber + 1
            const BaseboardInfo* boardInfo = reinterpret_cast<const BaseboardInfo*>(p);
            
            // Get the serial number string index
            BYTE serialIndex = boardInfo->SerialNumber;
            if (serialIndex == 0) return nullptr; // No serial number provided
            
            // Find the string section (it starts after the fixed structure)
            const char* stringSection = reinterpret_cast<const char*>(p + header->Length);
            
            // Find the Nth string (serialIndex)
            for (BYTE i = 1; i < serialIndex; i++) {
                stringSection += strlen(stringSection) + 1;
                // Check if we're still within bounds
                if (reinterpret_cast<const BYTE*>(stringSection) >= pEnd) return nullptr;
            }
            
            return stringSection; // Return pointer to the serial number string
        }
        
        // Move to next structure
        // First, skip the fixed part
        p += header->Length;
        
        // Then, skip the string-set (terminated by double NULL)
        while (p < pEnd && (p[0] != 0 || p[1] != 0)) {
            p += strlen(reinterpret_cast<const char*>(p)) + 1;
        }
        
        // Skip the terminating double NULL
        p += 2;
    }
    
    return nullptr; // No baseboard serial found
}

// --- Hook function ---

// Hook function for GetSystemFirmwareTable
UINT WINAPI Hooked_GetSystemFirmwareTable(DWORD FirmwareTableProviderSignature, 
                                           DWORD FirmwareTableID, 
                                           PVOID pFirmwareTableBuffer, 
                                           DWORD BufferSize) {
    // First, call the original function to get the actual data
    UINT result = Real_GetSystemFirmwareTable(FirmwareTableProviderSignature, 
                                             FirmwareTableID, 
                                             pFirmwareTableBuffer, 
                                             BufferSize);
    
    // Only modify the data if:
    // 1. The call was successful
    // 2. We're looking at SMBIOS data ('RSMB')
    // 3. We have a buffer to work with
    // 4. The buffer size is sufficient
    if (result > 0 && 
        FirmwareTableProviderSignature == 'RSMB' && 
        pFirmwareTableBuffer != nullptr && 
        BufferSize >= sizeof(RawSMBIOSData)) {
        
        RawSMBIOSData* pSmbios = static_cast<RawSMBIOSData*>(pFirmwareTableBuffer);
        
        // Search for the motherboard serial number in the SMBIOS data
        const char* originalSerial = ExtractMotherboardSerial(pSmbios);
        
        if (originalSerial) {
            // Get the length of the original serial number
            size_t originalSerialLen = strlen(originalSerial);
            
            // Convert our spoofed serial to ANSI for comparison and modification
            char spoofedSerialA[256];
            size_t convertedChars = 0;
            wcstombs_s(&convertedChars, spoofedSerialA, sizeof(spoofedSerialA), 
                      g_motherboardSerial, _TRUNCATE);
            
            // Get the length of our spoofed serial
            size_t spoofedSerialLen = strlen(spoofedSerialA);
            
            // Only overwrite if the spoofed serial will fit in the same space
            // (We don't want to modify the structure of the SMBIOS data)
            if (spoofedSerialLen <= originalSerialLen) {
                // Copy our spoofed serial over the original
                strcpy_s(const_cast<char*>(originalSerial), originalSerialLen + 1, spoofedSerialA);
                
                // If our serial is shorter, null-terminate it properly
                if (spoofedSerialLen < originalSerialLen) {
                    const_cast<char*>(originalSerial)[spoofedSerialLen] = '\0';
                }
            }
        }
    }
    
    return result;
}

// --- API functions ---

// Function to set the spoofed motherboard serial
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

// Function to get a pointer to the original function for hooking
PVOID* GetRealGetSystemFirmwareTable() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable);
}

// Function to get a pointer to the hook function
PVOID GetHookedGetSystemFirmwareTable() {
    return reinterpret_cast<PVOID>(Hooked_GetSystemFirmwareTable);
}

// Function to get the spoofed motherboard serial for other modules
const WCHAR* GetSpoofedMotherboardSerial() {
    return g_motherboardSerial;
}
