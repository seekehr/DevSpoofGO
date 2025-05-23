#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#pragma pack(push, 1)
typedef struct {
    BYTE Type;
    BYTE Length;
    WORD Handle;
} SMBIOSStructHeader;

typedef struct {
    SMBIOSStructHeader Header;
    BYTE SocketDesignation;
    BYTE ProcessorType;
    BYTE ProcessorFamily;
    BYTE ProcessorManufacturer;
    DWORDLONG ProcessorID;
    BYTE ProcessorVersion;
    BYTE Voltage;
    WORD ExternalClock;
    WORD MaxSpeed;
    WORD CurrentSpeed;
    BYTE Status;
    BYTE ProcessorUpgrade;
    WORD L1CacheHandle;
    WORD L2CacheHandle;
    WORD L3CacheHandle;
    BYTE SerialNumber;
    BYTE AssetTag;
    BYTE PartNumber;
    BYTE CoreCount;
    BYTE CoreEnabled;
    BYTE ThreadCount;
    WORD ProcessorCharacteristics;
    WORD ProcessorFamily2;
    WORD CoreCount2;
    WORD CoreEnabled2;
    WORD ThreadCount2;
} ProcessorInfoStruct;
#pragma pack(pop)

void ModifySmbiosForProcessorId(PVOID pFirmwareTableBuffer, DWORD BufferSize);

extern "C" {
    __declspec(dllexport) void SetSpoofedProcessorId(const char* processorIdString);
}

void InitializeProcessorIdHooks(PVOID realGetSystemFirmwareTable);
PVOID* GetRealGetSystemFirmwareTableForProcessorId();