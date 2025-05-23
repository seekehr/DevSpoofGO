#include "wmi_handler.h"
#include "wmi_utils.h" 
#include "wmi_bios_serial.h"
#include "wmi_processor.h"
#include "wmi_physical_memory.h"
#include <windows.h>
#include <atlbase.h> 
#include <atlconv.h> 
#include <comdef.h> 
#include <ocidl.h> 
#include <objidl.h>
#include "../detours/include/detours.h" 

#ifndef IID_IConnectionPointContainer
static const IID IID_IConnectionPointContainer = {0xB196B284, 0xBAB4, 0x101A, {0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1F, 0x07}};
#endif

void LogMessage(const char* format, ...) {
    char buffer[256]; 
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

RealExecQueryType g_real_ExecQuery = nullptr;
RealCoCreateInstanceType g_real_CoCreateInstance = nullptr;
RealConnectServerType g_real_ConnectServer = nullptr;

RealCreateInstanceEnumType g_real_CreateInstanceEnum = nullptr;
RealGetObjectType g_real_GetObject = nullptr;

static thread_local bool g_isInConnectServer = false;
static thread_local bool g_inGoOleCall = false;

bool AttachHook(void** originalFunc, void* hookFunc) { 
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    LONG error = DetourAttach(originalFunc, hookFunc); 
    if (error != NO_ERROR) {
        OutputDebugStringA("[WMI_HANDLER] CRITICAL: DetourAttach failed\n"); 
        DetourTransactionAbort();
        return false;
    }
    error = DetourTransactionCommit();
    if (error != NO_ERROR) {
        OutputDebugStringA("[WMI_HANDLER] CRITICAL: DetourTransactionCommit failed\n"); 
        return false;
    }
    return true;
}

HRESULT STDMETHODCALLTYPE Hooked_ExecQuery(
    IWbemServices *pThis, const BSTR strQueryLanguage, const BSTR strQuery,
    long lFlags, IWbemContext *pCtx, IEnumWbemClassObject **ppEnum)
{
    if (!ppEnum || !pThis) return E_POINTER;
    *ppEnum = nullptr;

    bool handled = false;
    
    // Try to handle BIOS SerialNumber queries
    HRESULT hr_handler = Handle_ExecQuery_BiosSerial(pThis, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum, handled);
    if (handled) {
        return hr_handler;
    }
    
    // Try to handle Processor SerialNumber and ProcessorId queries
    hr_handler = Handle_ExecQuery_Processor(pThis, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum, handled);
    if (handled) {
        return hr_handler;
    }
    
    // Try to handle PhysicalMemory SerialNumber and PartNumber queries
    hr_handler = Handle_ExecQuery_PhysicalMemory(pThis, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum, handled);
    if (handled) {
        return hr_handler;
    }

    if (!g_real_ExecQuery) return E_FAIL; 
    return g_real_ExecQuery(pThis, strQueryLanguage, strQuery, lFlags, pCtx, ppEnum);
}

HRESULT STDMETHODCALLTYPE Hooked_GetObject(
    IWbemServices *pThis, const BSTR strObjectPath, long lFlags,
    IWbemContext *pCtx, IWbemClassObject **ppObject, IWbemCallResult **ppCallResult)
{
    if (!ppObject) return E_POINTER; 
    *ppObject = nullptr;
    if (ppCallResult) *ppCallResult = nullptr;

    bool handled = false;
    
    // Try to handle BIOS SerialNumber queries
    HRESULT hr_handler = Handle_GetObject_BiosSerial(pThis, strObjectPath, lFlags, pCtx, ppObject, ppCallResult, handled);
    if (handled) {
        return hr_handler;
    }
    
    // Try to handle Processor SerialNumber and ProcessorId queries
    hr_handler = Handle_GetObject_Processor(pThis, strObjectPath, lFlags, pCtx, ppObject, ppCallResult, handled);
    if (handled) {
        return hr_handler;
    }
    
    // Try to handle PhysicalMemory SerialNumber and PartNumber queries
    hr_handler = Handle_GetObject_PhysicalMemory(pThis, strObjectPath, lFlags, pCtx, ppObject, ppCallResult, handled);
    if (handled) {
        return hr_handler;
    }

    if (!g_real_GetObject) return E_FAIL;
    return g_real_GetObject(pThis, strObjectPath, lFlags, pCtx, ppObject, ppCallResult);
}

HRESULT STDMETHODCALLTYPE Hooked_CreateInstanceEnum(
    IWbemServices *pThis, const BSTR strFilter, long lFlags,
    IWbemContext *pCtx, IEnumWbemClassObject **ppEnum)
{
    if (!ppEnum || !pThis) return E_POINTER;
    *ppEnum = nullptr;

    bool handled = false;
    
    // Try to handle BIOS SerialNumber queries
    HRESULT hr_handler = Handle_CreateInstanceEnum_BiosSerial(pThis, strFilter, lFlags, pCtx, ppEnum, handled);
    if (handled) {
        return hr_handler; 
    }
    
    // Try to handle Processor SerialNumber and ProcessorId queries
    hr_handler = Handle_CreateInstanceEnum_Processor(pThis, strFilter, lFlags, pCtx, ppEnum, handled);
    if (handled) {
        return hr_handler;
    }
    
    // Try to handle PhysicalMemory SerialNumber and PartNumber queries
    hr_handler = Handle_CreateInstanceEnum_PhysicalMemory(pThis, strFilter, lFlags, pCtx, ppEnum, handled);
    if (handled) {
        return hr_handler;
    }
    
    if (!g_real_CreateInstanceEnum) return E_FAIL;
    return g_real_CreateInstanceEnum(pThis, strFilter, lFlags, pCtx, ppEnum);
}

HRESULT STDMETHODCALLTYPE Hooked_ConnectServer(
    IWbemLocator *pThisLocator, const BSTR strNetworkResource, const BSTR strUser,
    const BSTR strPassword, const BSTR strLocale, long lFlags,
    const BSTR strAuthority, IWbemContext *pCtx, IWbemServices **ppSvc)
{
    if (g_isInConnectServer) {
        return g_real_ConnectServer(pThisLocator, strNetworkResource, strUser, strPassword, 
                                 strLocale, lFlags, strAuthority, pCtx, ppSvc);
    }
    if (!ppSvc || !pThisLocator || !g_real_ConnectServer) return E_POINTER; 

    g_isInConnectServer = true;
    HRESULT hr_original_connect = E_FAIL;
    
    try {
        hr_original_connect = g_real_ConnectServer(pThisLocator, strNetworkResource, strUser, strPassword, 
                                 strLocale, lFlags, strAuthority, pCtx, ppSvc);
    } catch (...) {
        g_isInConnectServer = false;
        return E_FAIL;
    }

    if (SUCCEEDED(hr_original_connect) && ppSvc && *ppSvc) {
        IWbemServices* pWmiService = *ppSvc;
        PVOID* vtable = *(PVOID**)pWmiService;

        if (vtable) {

            if (!g_real_ExecQuery) {
                RealExecQueryType originalExecQuery = (RealExecQueryType)vtable[20]; 
                if (AttachHook(&(PVOID&)originalExecQuery, Hooked_ExecQuery)) {
                    g_real_ExecQuery = originalExecQuery;
                }
            }

            if (!g_real_GetObject) {
                RealGetObjectType originalGetObject = (RealGetObjectType)vtable[6]; 
                if (AttachHook(&(PVOID&)originalGetObject, Hooked_GetObject)) {
                    g_real_GetObject = originalGetObject;
                }
            }

            if (!g_real_CreateInstanceEnum) {
                RealCreateInstanceEnumType originalCreateInstanceEnum = (RealCreateInstanceEnumType)vtable[18]; 
                if (AttachHook(&(PVOID&)originalCreateInstanceEnum, Hooked_CreateInstanceEnum)) {
                    g_real_CreateInstanceEnum = originalCreateInstanceEnum;
                }
            }
        }
    }
    g_isInConnectServer = false;
    return hr_original_connect;
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
    
    HRESULT hr_original_cci = g_real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    if (SUCCEEDED(hr_original_cci) && ppv && *ppv && IsEqualCLSID(rclsid, CLSID_WbemLocator)) {
        IWbemLocator* pLocator = static_cast<IWbemLocator*>(*ppv);
        PVOID* vtable = *(PVOID**)pLocator;

        if (vtable && !g_real_ConnectServer) { 
            RealConnectServerType originalConnectServer = (RealConnectServerType)vtable[3]; 
            if (AttachHook(&(PVOID&)originalConnectServer, Hooked_ConnectServer)) {
                 g_real_ConnectServer = originalConnectServer;
            }
        }
    }
    g_inGoOleCall = false;
    return hr_original_cci;
}

BOOL InitializeWMIHooks() {
    g_real_CoCreateInstance = (RealCoCreateInstanceType)DetourFindFunction("ole32.dll", "CoCreateInstance");
    if (!g_real_CoCreateInstance) {
        LogMessage("[WMI_HANDLER] ERROR: DetourFindFunction failed for CoCreateInstance.\n");
        return FALSE;
    }

    if (!AttachHook(&(PVOID&)g_real_CoCreateInstance, Hooked_CoCreateInstance_For_Locator)) {
        LogMessage("[WMI_HANDLER] CRITICAL: Failed to hook CoCreateInstance for WbemLocator.\n");
    }
    return TRUE;
}

void CleanupWMIHooks() {
    if (g_real_CoCreateInstance) {
        DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_CoCreateInstance, Hooked_CoCreateInstance_For_Locator);
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) LogMessage("[WMI_HANDLER] ERROR: Failed to commit CoCreateInstance detach: %ld\n", error);
        g_real_CoCreateInstance = nullptr; 
    }
    if (g_real_ConnectServer) {
        DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_ConnectServer, Hooked_ConnectServer);
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) LogMessage("[WMI_HANDLER] ERROR: Failed to commit ConnectServer detach: %ld\n", error);
        g_real_ConnectServer = nullptr;
    }

    if (g_real_ExecQuery) { 
        DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_ExecQuery, Hooked_ExecQuery);
        LONG error = DetourTransactionCommit(); 
        if (error != NO_ERROR) LogMessage("[WMI_HANDLER] ERROR: Failed to commit ExecQuery detach: %ld\n", error);
        g_real_ExecQuery = nullptr;
    }

    if (g_real_GetObject) { 
        DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_GetObject, Hooked_GetObject);
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) LogMessage("[WMI_HANDLER] ERROR: Failed to commit GetObject detach: %ld\n", error);
        g_real_GetObject = nullptr;
    }

    if (g_real_CreateInstanceEnum) {
        DetourTransactionBegin(); DetourUpdateThread(GetCurrentThread());
        DetourDetach(&(PVOID&)g_real_CreateInstanceEnum, Hooked_CreateInstanceEnum);
        LONG error = DetourTransactionCommit();
        if (error != NO_ERROR) LogMessage("[WMI_HANDLER] ERROR: Failed to commit CreateInstanceEnum detach: %ld\n", error);
        g_real_CreateInstanceEnum = nullptr;
    }
}
