#include "disk_serial.h"
#include <windows.h>
#include <winioctl.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../../detours/include/detours.h" // Microsoft Detours library

// --- Global variables ---
static WCHAR g_spoofedDiskSerialW[256] = L"SPOOFED_DISK_SERIAL_123";


// --- Exported functions ---
extern "C" {
    __declspec(dllexport) void SetSpoofedDiskSerial(const char* serial) {
        if (serial) {
            size_t convertedChars = 0;
            mbstowcs_s(&convertedChars, g_spoofedDiskSerialW, sizeof(g_spoofedDiskSerialW) / sizeof(WCHAR), serial, _TRUNCATE);
        } else {
            g_spoofedDiskSerialW[0] = L'\0';
        }
    }
}


// --- Hooked function ---
BOOL WINAPI HookedDeviceIoControlForDiskSerial(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped) {

    if (!g_real_DeviceIoControl) {
        OutputDebugStringW(L"DS_SPOOF_ERROR: CRITICAL - g_real_DeviceIoControl is NULL! Calling API directly.");
        return DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
    }

    BOOL result = g_real_DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);

    if (result &&
        dwIoControlCode == IOCTL_STORAGE_QUERY_PROPERTY &&
        lpInBuffer != nullptr && nInBufferSize >= sizeof(STORAGE_PROPERTY_QUERY) &&
        lpOutBuffer != nullptr && lpBytesReturned != nullptr && *lpBytesReturned > 0 &&
        g_spoofedDiskSerialW[0] != L'\0') {
        STORAGE_PROPERTY_QUERY* pQuery = static_cast<STORAGE_PROPERTY_QUERY*>(lpInBuffer);
        if (pQuery->PropertyId == StorageDeviceProperty && pQuery->QueryType == PropertyStandardQuery) {
            if (*lpBytesReturned >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
                STORAGE_DEVICE_DESCRIPTOR* pDevDesc = static_cast<STORAGE_DEVICE_DESCRIPTOR*>(lpOutBuffer);

                if (pDevDesc->SerialNumberOffset != 0 &&
                    pDevDesc->SerialNumberOffset < *lpBytesReturned) {
                    char* originalSerialLocation = (char*)lpOutBuffer + pDevDesc->SerialNumberOffset;
                    size_t availableSpace = *lpBytesReturned - pDevDesc->SerialNumberOffset;

                    if (availableSpace > 0) {
                        char spoofedSerialA[256];
                        size_t convertedChars = 0;
                        wcstombs_s(&convertedChars, spoofedSerialA, sizeof(spoofedSerialA), g_spoofedDiskSerialW, _TRUNCATE);
                        
                        size_t spoofedSerialLen = strlen(spoofedSerialA);

                        if (spoofedSerialLen > 0 && spoofedSerialLen < availableSpace) {
                            strncpy_s(originalSerialLocation, availableSpace, spoofedSerialA, spoofedSerialLen);
                            originalSerialLocation[spoofedSerialLen] = '\0';
                        } else if (spoofedSerialLen > 0 && spoofedSerialLen >= availableSpace) {
                            strncpy_s(originalSerialLocation, availableSpace, spoofedSerialA, availableSpace - 1);
                            originalSerialLocation[availableSpace - 1] = '\0';
                        }
                    }
                }
            }
        }
    }
    return result;
}

// Function to initialize the hook for DeviceIoControl for disk serial spoofing
bool InitializeDiskSerialHooks() {
    if (g_real_DeviceIoControl == DeviceIoControl) { // Check if it's still pointing to the original API
        LONG error = DetourAttach(&(PVOID&)g_real_DeviceIoControl, HookedDeviceIoControlForDiskSerial);
        if (error != NO_ERROR) {
            OutputDebugStringW(L"DS_SPOOF_ERROR: Failed to attach DeviceIoControl (original).");
            return false;
        }
        return true;
    } else if (g_real_DeviceIoControl != HookedDeviceIoControlForDiskSerial) {
        OutputDebugStringW(L"DS_SPOOF_WARNING: DeviceIoControl appears to be already hooked by another module. Attempting to chain.");
        LONG error = DetourAttach(&(PVOID&)g_real_DeviceIoControl, HookedDeviceIoControlForDiskSerial);
        if (error != NO_ERROR) {
            OutputDebugStringW(L"DS_SPOOF_ERROR: Failed to attach DeviceIoControl (chaining attempt).");
            return false;
        }
        return true;
    } else {
        OutputDebugStringW(L"DS_SPOOF_INFO: DeviceIoControl for Disk Serial already hooked by this module.");
        return true;
    }
}

// Function to cleanup the hook for DeviceIoControl
void CleanupDiskSerialHooks() {
    LONG error = DetourDetach(&(PVOID&)g_real_DeviceIoControl, HookedDeviceIoControlForDiskSerial);
    if (error != NO_ERROR) {
        OutputDebugStringW(L"DS_SPOOF_ERROR: Failed to detach DeviceIoControl for Disk Serial. May not have been hooked by us or other error.");
    } else {
        OutputDebugStringW(L"DS_SPOOF: DeviceIoControl for Disk Serial unhooked or was not attached by this module.");
    }
}
