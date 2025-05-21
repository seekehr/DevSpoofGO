#pragma once
#include <windows.h>
#include <wbemidl.h> 
#include <Unknwn.h> 

// Function pointer types
typedef HRESULT (STDMETHODCALLTYPE *RealExecQueryType)(
    IWbemServices*, const BSTR, const BSTR, long, IWbemContext*, IEnumWbemClassObject**);

typedef HRESULT (STDMETHODCALLTYPE *RealConnectServerType)(
    IWbemLocator*, const BSTR, const BSTR, const BSTR, const BSTR, long,
    const BSTR, IWbemContext*, IWbemServices**);

typedef HRESULT (WINAPI *RealCoCreateInstanceType)(
    REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);

typedef HRESULT (STDMETHODCALLTYPE *RealCreateInstanceEnumType)(
    IWbemServices*, const BSTR, long, IWbemContext*, IEnumWbemClassObject**);

typedef HRESULT (STDMETHODCALLTYPE *RealGetObjectType)(
    IWbemServices*, const BSTR, long, IWbemContext*, IWbemClassObject**, IWbemCallResult**);

// Global pointers to hold the addresses of
extern RealCoCreateInstanceType g_real_CoCreateInstance;
extern RealConnectServerType g_real_ConnectServer;

// Global pointers for the original CreateInstanceEnum and IEnumWbemClassObject::Next functions
extern RealCreateInstanceEnumType g_real_CreateInstanceEnum;
extern RealGetObjectType g_real_GetObject;
extern RealExecQueryType g_real_ExecQuery;

// Declaration for the logging function
void LogMessage(const char* format, ...);

// Our hooked versions
HRESULT STDMETHODCALLTYPE Hooked_ExecQuery(
    IWbemServices *pThis,
    const BSTR strQueryLanguage,
    const BSTR strQuery,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum
);

HRESULT WINAPI Hooked_CoCreateInstance(
    REFCLSID rclsid,
    LPUNKNOWN pUnkOuter,
    DWORD dwClsContext,
    REFIID riid,
    LPVOID *ppv
);

// Our hooked version of CoCreateInstance specifically for finding IWbemLocator
HRESULT WINAPI Hooked_CoCreateInstance_For_Locator(
    REFCLSID rclsid,
    LPUNKNOWN pUnkOuter,
    DWORD dwClsContext,
    REFIID riid,
    LPVOID *ppv
);

// Our hooked version of ConnectServer
HRESULT STDMETHODCALLTYPE Hooked_ConnectServer(
    IWbemLocator *pThis,
    const BSTR strNetworkResource,
    const BSTR strUser,
    const BSTR strPassword,
    const BSTR strLocale,
    long lFlags,
    const BSTR strAuthority,
    IWbemContext *pCtx,
    IWbemServices **ppSvc
);

// Our hooked version of CreateInstanceEnum
HRESULT STDMETHODCALLTYPE Hooked_CreateInstanceEnum(
    IWbemServices *pThis,
    const BSTR strFilter,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum
);

// Our hooked version of GetObjectType
HRESULT STDMETHODCALLTYPE Hooked_GetObject(
    IWbemServices *pThis,
    const BSTR strObjectPath,
    long lFlags,
    IWbemContext *pCtx,
    IWbemClassObject **ppObject,
    IWbemCallResult **ppCallResult
);

// WMI hook initialization/cleanup functions
BOOL InitializeWMIHooks();
void CleanupWMIHooks();
