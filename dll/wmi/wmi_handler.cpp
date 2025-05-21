#include "wmi_handler.h"
#include "bios_serial_wmi.h" // Include the new module
#include <windows.h>
#include <atlbase.h> // For CComPtr, CComVariant, CComBSTR
#include <atlconv.h> // For string conversions like CW2A
#include <vector>
#include <string>
#include <algorithm> // For std::transform
#include <comdef.h> // For _com_error
#include <ocidl.h> // For IConnectionPointContainer
#include <objidl.h> // For IClientSecurity and IID_IClientSecurity
#include "../detours/include/detours.h" // For DetourAttach, DetourTransactionBegin, etc.

#ifndef IID_IConnectionPointContainer
// {B196B284-BAB4-101A-B69C-00AA00341F07}
static const IID IID_IConnectionPointContainer = {0xB196B284, 0xBAB4, 0x101A, {0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1F, 0x07}};
#endif

// Keep LogMessage for truly essential error reporting
void LogMessage(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

// Define the WbemServices CLSID which is missing from standard headers in some cases
// static const CLSID CLSID_WMIServices = {0x4590f811, 0x1d3a, 0x11d0, {0x89, 0x1f, 0x00, 0xaa, 0x00, 0x4b, 0x2e, 0x24}}; // Not used directly in minimal

// Trampoline for the original ExecQuery function
RealExecQueryType g_real_ExecQuery = nullptr;
// Trampoline for the original CoCreateInstance function
RealCoCreateInstanceType g_real_CoCreateInstance = nullptr;
// Trampoline for the original ConnectServer function
RealConnectServerType g_real_ConnectServer = nullptr;

// Add to top of file with other global definitions
typedef HRESULT (STDMETHODCALLTYPE* RealCreateInstanceEnumType)(
    IWbemServices*, const BSTR, long, IWbemContext*, IEnumWbemClassObject**);
RealCreateInstanceEnumType g_real_CreateInstanceEnum = nullptr;

// Add GetObject type definition and global pointer
typedef HRESULT (STDMETHODCALLTYPE* RealGetObjectType)(
    IWbemServices*, const BSTR, long, IWbemContext*, IWbemClassObject**, IWbemCallResult**);
RealGetObjectType g_real_GetObject = nullptr;

// Add a thread-local recursion guard
static thread_local bool g_isInConnectServer = false;

// Add a thread-local flag for Go runtime safety
static thread_local bool g_inGoOleCall = false;

// Helper function to attach a hook using Detours
bool AttachHook(void** originalFunc, void* hookFunc) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    LONG error = DetourAttach(&(PVOID&)*originalFunc, hookFunc);
    if (error != NO_ERROR) {
        LogMessage("[WMI_HANDLER] CRITICAL: DetourAttach failed: %d\n", error);
        DetourTransactionAbort();
        return false;
    }
    error = DetourTransactionCommit();
    if (error != NO_ERROR) {
        LogMessage("[WMI_HANDLER] CRITICAL: DetourTransactionCommit failed: %d\n", error);
        return false;
    }
    return true;
}

// Mock classes and related helper functions (IsBIOSOrBaseboardSerialQuery, IsBIOSOrBaseboardPath) 
// have been moved to the bios_serial_wmi module.

// Hook implementations
HRESULT STDMETHODCALLTYPE Hooked_ExecQuery(
    IWbemServices *pThis, const BSTR strQueryLanguage, const BSTR strQuery,
    long lFlags, IWbemContext *pCtx, IEnumWbemClassObject **ppEnum)
{
    if (!ppEnum || !pThis) return E_POINTER;
    *ppEnum = nullptr;

    bool handled = false;
    HRESULT hr = Handle_ExecQuery_BiosSerial(pThis, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum, handled);
    
    if (handled) {
        return hr;
    }

    if (!g_real_ExecQuery) {
        LogMessage("[WMI_HANDLER] CRITICAL: Original ExecQuery is NULL.\n");
        return E_FAIL; 
    }
    return g_real_ExecQuery(pThis, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum);
}

HRESULT STDMETHODCALLTYPE Hooked_GetObject(
    IWbemServices *pThis, const BSTR strObjectPath, long lFlags,
    IWbemContext *pCtx, IWbemClassObject **ppObject, IWbemCallResult **ppCallResult)
{
    if (!ppObject) return E_POINTER; // ppCallResult can be NULL
    *ppObject = nullptr;
    if (ppCallResult) *ppCallResult = nullptr;

    bool handled = false;
    HRESULT hr = Handle_GetObject_BiosSerial(pThis, strObjectPath, lFlags, pCtx, ppObject, ppCallResult, handled);

    if (handled) {
        return hr; // BiosSerial module handled it
    }

    if (!g_real_GetObject) {
        LogMessage("[WMI_HANDLER] CRITICAL: Original GetObject is NULL.\n");
        return E_FAIL;
    }
    return g_real_GetObject(pThis, strObjectPath, lFlags, pCtx, ppObject, ppCallResult);
}

HRESULT STDMETHODCALLTYPE Hooked_CreateInstanceEnum(
    IWbemServices *pThis, const BSTR strFilter, long lFlags,
    IWbemContext *pCtx, IEnumWbemClassObject **ppEnum)
{
    if (!ppEnum || !pThis) return E_POINTER;
    *ppEnum = nullptr;

    bool handled = false;
    HRESULT hr = Handle_CreateInstanceEnum_BiosSerial(pThis, strFilter, lFlags, pCtx, ppEnum, handled);

    if (handled) {
        return hr; // BiosSerial module handled it
    }
    
    if (!g_real_CreateInstanceEnum) {
        LogMessage("[WMI_HANDLER] CRITICAL: Original CreateInstanceEnum is NULL.\n");
        return E_FAIL;
    }
    return g_real_CreateInstanceEnum(pThis, strFilter, lFlags, pCtx, ppEnum);
}

HRESULT STDMETHODCALLTYPE Hooked_ConnectServer(
    IWbemLocator *pThis, const BSTR strNetworkResource, const BSTR strUser,
    const BSTR strPassword, const BSTR strLocale, long lFlags,
    const BSTR strAuthority, IWbemContext *pCtx, IWbemServices **ppSvc)
{
    if (g_isInConnectServer) {
        return g_real_ConnectServer(pThis, strNetworkResource, strUser, strPassword, 
                                 strLocale, lFlags, strAuthority, pCtx, ppSvc);
    }
    if (!ppSvc || !pThis || !g_real_ConnectServer) return E_POINTER;

    g_isInConnectServer = true;
    HRESULT hr = E_FAIL;
    
    try {
        hr = g_real_ConnectServer(pThis, strNetworkResource, strUser, strPassword, 
                                 strLocale, lFlags, strAuthority, pCtx, ppSvc);
    } catch (...) {
        LogMessage("[WMI_HANDLER] CRITICAL: Exception in original ConnectServer call\n");
        g_isInConnectServer = false;
        return E_FAIL;
    }

    if (SUCCEEDED(hr) && ppSvc && *ppSvc) {
        IWbemServices* pWmiService = *ppSvc;
        PVOID* vtable = *(PVOID**)pWmiService;

        if (vtable) {
            DetourTransactionBegin();
            bool commitTransaction = false;

            if (!g_real_ExecQuery) {
                RealExecQueryType originalExecQuery = (RealExecQueryType)vtable[20]; 
                if (DetourAttach(&(PVOID&)originalExecQuery, Hooked_ExecQuery) == NO_ERROR) {
                    g_real_ExecQuery = originalExecQuery;
                    commitTransaction = true;
                } else {
                    LogMessage("[WMI_HANDLER] CRITICAL: Failed to attach hook for ExecQuery\n");
                }
            }

            if (!g_real_GetObject) {
                RealGetObjectType originalGetObject = (RealGetObjectType)vtable[6];
                if (DetourAttach(&(PVOID&)originalGetObject, Hooked_GetObject) == NO_ERROR) {
                    g_real_GetObject = originalGetObject;
                    commitTransaction = true;
                } else {
                     LogMessage("[WMI_HANDLER] CRITICAL: Failed to attach hook for GetObject\n");
                }
            }

            if (!g_real_CreateInstanceEnum) {
                RealCreateInstanceEnumType originalCreateInstanceEnum = (RealCreateInstanceEnumType)vtable[14];
                if (DetourAttach(&(PVOID&)originalCreateInstanceEnum, Hooked_CreateInstanceEnum) == NO_ERROR) {
                    g_real_CreateInstanceEnum = originalCreateInstanceEnum;
                    commitTransaction = true;
                } else {
                    LogMessage("[WMI_HANDLER] CRITICAL: Failed to attach hook for CreateInstanceEnum\n");
                }
            }
            
            if (commitTransaction) {
                if (DetourTransactionCommit() != NO_ERROR) {
                     LogMessage("[WMI_HANDLER] CRITICAL: Hook transaction commit failed in ConnectServer\n");
                }
            } else {
                // If no hooks were attached in this run, abort any pending transaction
                DetourTransactionAbort();
            }
        }
    }
    g_isInConnectServer = false;
    return hr;
}

HRESULT WINAPI Hooked_CoCreateInstance_For_Locator(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext,
    REFIID riid, LPVOID *ppv)
{
    if (!ppv || !g_real_CoCreateInstance) return E_POINTER;
    
    if (g_inGoOleCall) {
        return g_real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
    }
    g_inGoOleCall = true;
    
    HRESULT hr = g_real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    if (SUCCEEDED(hr) && ppv && *ppv && IsEqualCLSID(rclsid, CLSID_WbemLocator)) {
        IWbemLocator* pLocator = static_cast<IWbemLocator*>(*ppv);
        PVOID* vtable = *(PVOID**)pLocator;

        if (vtable && !g_real_ConnectServer) { 
            RealConnectServerType originalConnectServer = (RealConnectServerType)vtable[3]; 

            DetourTransactionBegin();
            if (DetourAttach(&(PVOID&)originalConnectServer, Hooked_ConnectServer) == NO_ERROR) {
                if (DetourTransactionCommit() == NO_ERROR) {
                    g_real_ConnectServer = originalConnectServer;
                } else {
                    LogMessage("[WMI_HANDLER] CRITICAL: Hook transaction commit failed for ConnectServer.\n");
                }
            } else {
                LogMessage("[WMI_HANDLER] CRITICAL: Failed to attach hook for ConnectServer.\n");
                DetourTransactionAbort();
            }
        }
    }
    g_inGoOleCall = false;
    return hr;
}

BOOL InitializeWMIHooks() {
    InitializeBiosSerialWMIModule(); 

    g_real_CoCreateInstance = (RealCoCreateInstanceType)DetourFindFunction("ole32.dll", "CoCreateInstance");
    if (!g_real_CoCreateInstance) {
        LogMessage("[WMI_HANDLER] CRITICAL: Failed to find CoCreateInstance in ole32.dll\n");
        return FALSE;
    }

    if (!AttachHook(&(PVOID&)g_real_CoCreateInstance, Hooked_CoCreateInstance_For_Locator)) {
        LogMessage("[WMI_HANDLER] CRITICAL: Failed to attach hook to CoCreateInstance (for Locator). Not fatal for all WMI, but locator-based init will fail.\n");
    }

    OutputDebugStringW(L"[WMI_HANDLER] WMI hooks initialized (CoCreateInstance for IWbemLocator hooked).\n");
    return TRUE;
}

void CleanupWMIHooks() {
    if (g_real_CoCreateInstance) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_CoCreateInstance, Hooked_CoCreateInstance_For_Locator);
        DetourTransactionCommit();
        g_real_CoCreateInstance = nullptr; 
    }

    if (g_real_ConnectServer) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_ConnectServer, Hooked_ConnectServer);
        DetourTransactionCommit();
        g_real_ConnectServer = nullptr;
    }

    if (g_real_ExecQuery) { 
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_ExecQuery, Hooked_ExecQuery);
        DetourTransactionCommit();
        g_real_ExecQuery = nullptr;
    }

    if (g_real_GetObject) { 
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_GetObject, Hooked_GetObject);
        DetourTransactionCommit();
        g_real_GetObject = nullptr;
    }

    if (g_real_CreateInstanceEnum) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_CreateInstanceEnum, Hooked_CreateInstanceEnum);
        DetourTransactionCommit();
        g_real_CreateInstanceEnum = nullptr;
    }
    
    CleanupBiosSerialWMIModule(); 
    OutputDebugStringW(L"[WMI_HANDLER] WMI hooks cleaned up.\n");
}
