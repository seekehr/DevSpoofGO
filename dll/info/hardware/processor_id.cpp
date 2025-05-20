#include "processor_id.h"
#include <windows.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

static DWORDLONG g_spoofedProcessorId = 0x0123456789ABCDEF;

static UINT (WINAPI *Real_GetSystemFirmwareTable_Processor)(DWORD, DWORD, PVOID, DWORD) = nullptr;

#pragma pack(push, 1)
typedef struct {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
} RawSMBIOSDataProcessor;
#pragma pack(pop)

const BYTE* FindNextSMBIOSStructure(const BYTE* p, const BYTE* pEnd) {
    if (!p || p >= pEnd) return nullptr;
    const SMBIOSStructHeader* header = reinterpret_cast<const SMBIOSStructHeader*>(p);
    if (header->Length == 0) return nullptr;
    p += header->Length;
    while (p < pEnd - 1) {
        if (p[0] == 0 && p[1] == 0) return p + 2;
        p++;
    }
    return nullptr;
}

void SetSpoofedProcessorId(const char* processorIdString) {
    if (processorIdString && processorIdString[0] != '\0') {
        char* endPtr = nullptr;
        DWORDLONG newId = strtoull(processorIdString, &endPtr, 16);
        if (endPtr != processorIdString && (*endPtr == '\0' || *endPtr == ' ')) {
            g_spoofedProcessorId = newId;
        } else {
            OutputDebugStringA("[ProcessorID] Failed to parse processorIdString.");
        }
    } else {
        OutputDebugStringA("[ProcessorID] processorIdString is null or empty, using default/zero.");
    }
}

void ModifySmbiosForProcessorId(PVOID pFirmwareTableBuffer, DWORD BufferSize) {
    if (!pFirmwareTableBuffer || BufferSize < sizeof(RawSMBIOSDataProcessor)) return;
    RawSMBIOSDataProcessor* pSmbios = static_cast<RawSMBIOSDataProcessor*>(pFirmwareTableBuffer);
    if (pSmbios->Length == 0 || pSmbios->Length > BufferSize - offsetof(RawSMBIOSDataProcessor, SMBIOSTableData)) return;
    const BYTE* tableData = pSmbios->SMBIOSTableData;
    const BYTE* pEnd = tableData + pSmbios->Length;
    const BYTE* p = tableData;
    while (p && p < pEnd && (p + sizeof(SMBIOSStructHeader) <= pEnd)) {
        const SMBIOSStructHeader* header = reinterpret_cast<const SMBIOSStructHeader*>(p);
        if (header->Length < sizeof(SMBIOSStructHeader)) break;
        if (header->Type == 4) {
            if (header->Length >= offsetof(ProcessorInfoStruct, ProcessorID) + sizeof(DWORDLONG)) {
                ProcessorInfoStruct* procInfo = reinterpret_cast<ProcessorInfoStruct*>(const_cast<BYTE*>(p));
                procInfo->ProcessorID = g_spoofedProcessorId;
            }
        }
        p = FindNextSMBIOSStructure(p, pEnd);
        if (!p) break;
    }
    OutputDebugStringW(L"PROCESSOR_ID: ModifySmbiosForProcessorId called.");
}

PVOID* GetRealGetSystemFirmwareTableForProcessorId() {
    return reinterpret_cast<PVOID*>(&Real_GetSystemFirmwareTable_Processor);
}

void InitializeProcessorIdHooks(PVOID realGetSystemFirmwareTable) {
    if (realGetSystemFirmwareTable) {
        Real_GetSystemFirmwareTable_Processor = reinterpret_cast<UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD)>(realGetSystemFirmwareTable);
    } else {
        OutputDebugStringW(L"PROCESSOR_ID_SPOOF: Init with NULL Real_GetSystemFirmwareTable pointer!");
    }
}