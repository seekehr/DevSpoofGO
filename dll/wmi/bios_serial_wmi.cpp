#include "bios_serial_wmi.h"
#include "../wmi_handler.h" 
#include <atlbase.h> 
#include <atlconv.h> 
#include <comdef.h>
#include <string>     

// IID_IConnectionPointContainer is often in ocidl.h
#include <ocidl.h> 

// Define IID_IClientSecurity if it's not available from standard headers in this context
#ifndef IID_IClientSecurity
// {0000013D-0000-0000-C000-000000000046}
static const IID IID_IClientSecurity = {0x0000013D, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
#endif

// IID_IConnectionPointContainer might also need to be defined if not pulled in correctly
#ifndef IID_IConnectionPointContainer
// {B196B284-BAB4-101A-B69C-00AA00341F07}
static const IID IID_IConnectionPointContainer_Local = {0xB196B284, 0xBAB4, 0x101A, {0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1F, 0x07}};
#else
static const IID& IID_IConnectionPointContainer_Local = IID_IConnectionPointContainer;
#endif

const wchar_t* BIOS_SPOOFED_SERIAL_NUMBER = L"F122RCX0031FBNC"; // Specific to this module

// MockClientSecurity class definition (moved from wmi_handler.cpp)
class MockClientSecurity : public IClientSecurity {
private:
    ULONG m_cRef;
public:
    MockClientSecurity() : m_cRef(1) {}
    ~MockClientSecurity() {}

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        if (riid == IID_IUnknown || riid == IID_IClientSecurity) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }
    STDMETHODIMP QueryBlanket(IUnknown *pProxy, DWORD *pAuthnSvc, DWORD *pAuthzSvc, OLECHAR **pServerPrincipalName, DWORD *pAuthnLevel, DWORD *pImpLevel, void **pAuthInfo, DWORD *pCapabilities) {
        if (pAuthnSvc) *pAuthnSvc = RPC_C_AUTHN_WINNT;
        if (pAuthzSvc) *pAuthzSvc = RPC_C_AUTHZ_NONE;
        if (pServerPrincipalName) *pServerPrincipalName = nullptr; 
        if (pAuthnLevel) *pAuthnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY; 
        if (pImpLevel) *pImpLevel = RPC_C_IMP_LEVEL_IMPERSONATE;
        if (pAuthInfo) *pAuthInfo = nullptr; 
        if (pCapabilities) *pCapabilities = EOAC_NONE;
        return S_OK;
    }
    STDMETHODIMP SetBlanket(IUnknown *pProxy, DWORD AuthnSvc, DWORD AuthzSvc, OLECHAR *pServerPrincipalName, DWORD AuthnLevel, DWORD ImpLevel, void *pAuthInfo, DWORD Capabilities) {
        return S_OK;
    }
    STDMETHODIMP CopyProxy(IUnknown *pProxy, IUnknown **ppCopy) {
        if (!ppCopy) return E_POINTER;
        *ppCopy = nullptr;
        return E_NOTIMPL;
    }
};

// MockConnectionPointContainer class definition (moved from wmi_handler.cpp)
class MockConnectionPointContainer : public IConnectionPointContainer {
private:
    ULONG m_cRef;
public:
    MockConnectionPointContainer() : m_cRef(1) {}
    ~MockConnectionPointContainer() {}

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        if (riid == IID_IUnknown || riid == IID_IConnectionPointContainer_Local) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }
    STDMETHODIMP EnumConnectionPoints(IEnumConnectionPoints **ppEnum) {
        if (ppEnum) *ppEnum = NULL;
        return E_NOTIMPL;
    }
    STDMETHODIMP FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP) {
        if (ppCP) *ppCP = NULL;
        return E_NOTIMPL;
    }
};

// MockWbemClassObject class definition (moved from wmi_handler.cpp)
class MockWbemClassObject : public IWbemClassObject {
private:
    ULONG m_cRef;
    std::wstring m_serialNumber;
    std::wstring m_class; 
    MockConnectionPointContainer* m_pCPC;
    IClientSecurity* m_pClientSecurity;

public:
    MockWbemClassObject(const wchar_t* serial, const wchar_t* className = L"Win32_BIOS") 
        : m_cRef(1), m_serialNumber(serial), m_class(className), 
          m_pCPC(new MockConnectionPointContainer()),
          m_pClientSecurity(new MockClientSecurity()) {}

    ~MockWbemClassObject() {
        if (m_pCPC) m_pCPC->Release();
        if (m_pClientSecurity) m_pClientSecurity->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        
        if (riid == IID_IUnknown || riid == IID_IWbemClassObject) {
            *ppvObject = static_cast<IWbemClassObject*>(this);
            AddRef();
            return S_OK;
        } else if (riid == IID_IConnectionPointContainer_Local) {
            *ppvObject = m_pCPC;
            m_pCPC->AddRef();
            return S_OK;
        } else if (riid == IID_IClientSecurity) {
            *ppvObject = m_pClientSecurity;
            m_pClientSecurity->AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    STDMETHODIMP Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE *pType, long *plFlavor) {
        if (!wszName) return E_INVALIDARG;
        if (_wcsicmp(wszName, L"SerialNumber") == 0) {
            if (!pVal) { 
                 if (pType) *pType = CIM_STRING;
                 if (plFlavor) *plFlavor = WBEM_FLAVOR_ORIGIN_PROPAGATED; 
                 return S_OK; 
            }
            VariantInit(pVal);
            pVal->vt = VT_BSTR;
            pVal->bstrVal = SysAllocString(m_serialNumber.c_str());
            if (!pVal->bstrVal) {
                OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: Failed to allocate BSTR for SerialNumber. E_OUTOFMEMORY.\n");
                return E_OUTOFMEMORY;
            }
            if (pType) *pType = CIM_STRING;
            return S_OK;
        }
        if (!pVal) return E_POINTER; 
        return WBEM_E_NOT_FOUND;
    }
    STDMETHODIMP Put(LPCWSTR, long, VARIANT*, CIMTYPE) { return E_NOTIMPL; }
    STDMETHODIMP Delete(LPCWSTR) { return E_NOTIMPL; }
    STDMETHODIMP GetNames(LPCWSTR, long, VARIANT*, SAFEARRAY **pNames) {
        if (!pNames) return E_POINTER;
        SAFEARRAYBOUND bounds; bounds.lLbound = 0; bounds.cElements = 1;
        *pNames = SafeArrayCreate(VT_BSTR, 1, &bounds);
        if (!*pNames) return E_OUTOFMEMORY;
        BSTR prop = SysAllocString(L"SerialNumber");
        if (!prop) { SafeArrayDestroy(*pNames); *pNames = nullptr; return E_OUTOFMEMORY; }
        long index = 0; HRESULT hr = SafeArrayPutElement(*pNames, &index, prop);
        SysFreeString(prop); return SUCCEEDED(hr) ? S_OK : hr;
    }
    STDMETHODIMP BeginEnumeration(long) { return WBEM_S_NO_ERROR; } 
    STDMETHODIMP Next(long, BSTR*, VARIANT*, CIMTYPE*, long*) { return WBEM_S_NO_MORE_DATA; }
    STDMETHODIMP EndEnumeration() { return WBEM_S_NO_ERROR; }
    STDMETHODIMP GetPropertyQualifierSet(LPCWSTR, IWbemQualifierSet**) { return E_NOTIMPL; }
    STDMETHODIMP Clone(IWbemClassObject **ppCopy) {
        if (!ppCopy) return E_POINTER;
        *ppCopy = new MockWbemClassObject(m_serialNumber.c_str(), m_class.c_str());
        return (*ppCopy) ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP GetObjectText(long, BSTR*) { return E_NOTIMPL; }
    STDMETHODIMP SpawnDerivedClass(long, IWbemClassObject**) { return E_NOTIMPL; }
    STDMETHODIMP SpawnInstance(long, IWbemClassObject**) { return E_NOTIMPL; }
    STDMETHODIMP CompareTo(long, IWbemClassObject*) { return E_NOTIMPL; }
    STDMETHODIMP GetPropertyOrigin(LPCWSTR, BSTR*) { return E_NOTIMPL; }
    STDMETHODIMP InheritsFrom(LPCWSTR) { return E_NOTIMPL; }
    STDMETHODIMP GetMethod(LPCWSTR, long, IWbemClassObject**, IWbemClassObject**) { return E_NOTIMPL; }
    STDMETHODIMP PutMethod(LPCWSTR, long, IWbemClassObject*, IWbemClassObject*) { return E_NOTIMPL; }
    STDMETHODIMP DeleteMethod(LPCWSTR) { return E_NOTIMPL; }
    STDMETHODIMP BeginMethodEnumeration(long) { return E_NOTIMPL; }
    STDMETHODIMP NextMethod(long, BSTR*, IWbemClassObject**, IWbemClassObject**) { return E_NOTIMPL; }
    STDMETHODIMP EndMethodEnumeration() { return S_OK; }
    STDMETHODIMP GetMethodQualifierSet(LPCWSTR, IWbemQualifierSet**) { return E_NOTIMPL; }
    STDMETHODIMP GetMethodOrigin(LPCWSTR, BSTR*) { return E_NOTIMPL; }
    STDMETHODIMP GetQualifierSet(IWbemQualifierSet**) { return E_NOTIMPL; }
};

// MockEnumWbemClassObject class definition (moved from wmi_handler.cpp)
class MockEnumWbemClassObject : public IEnumWbemClassObject {
private:
    ULONG m_cRef;
    bool m_hasReturned;
    IWbemClassObject* m_spoof;
    std::wstring m_className;
    MockConnectionPointContainer* m_pCPC;
    IClientSecurity* m_pClientSecurity;

public:
    MockEnumWbemClassObject(const wchar_t* serial, const wchar_t* className = L"Win32_BIOS") 
        : m_cRef(1), m_hasReturned(false), m_className(className),
          m_pCPC(new MockConnectionPointContainer()),
          m_pClientSecurity(new MockClientSecurity()) {
        m_spoof = new MockWbemClassObject(serial, className);
        if (!m_spoof) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: Failed to create MockWbemClassObject in MockEnumWbemClassObject. E_OUTOFMEMORY.\n");
        }
    }
    
    ~MockEnumWbemClassObject() {
        if (m_spoof) m_spoof->Release();
        if (m_pCPC) m_pCPC->Release();
        if (m_pClientSecurity) m_pClientSecurity->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;
        if (riid == IID_IUnknown || riid == IID_IEnumWbemClassObject) {
            *ppvObject = static_cast<IEnumWbemClassObject*>(this);
            AddRef();
            return S_OK;
        } else if (riid == IID_IConnectionPointContainer_Local) {
            *ppvObject = m_pCPC;
            m_pCPC->AddRef();
            return S_OK;
        } else if (riid == IID_IClientSecurity) {
            *ppvObject = m_pClientSecurity;
            m_pClientSecurity->AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&m_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG cRef = InterlockedDecrement(&m_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }
    STDMETHODIMP Reset() { m_hasReturned = false; return S_OK; }
    STDMETHODIMP Next(long, ULONG uCount, IWbemClassObject **apObjects, ULONG *puReturned) {
        if (!apObjects || !puReturned) return WBEM_E_INVALID_PARAMETER;
        *puReturned = 0; if (uCount == 0) return WBEM_S_NO_ERROR;
        for (ULONG i = 0; i < uCount; i++) apObjects[i] = nullptr;
        if (m_hasReturned || !m_spoof) return WBEM_S_FALSE;
        apObjects[0] = m_spoof;
        apObjects[0]->AddRef(); 
        *puReturned = 1; m_hasReturned = true;
        return WBEM_S_NO_ERROR;
    }
    STDMETHODIMP Clone(IEnumWbemClassObject **ppEnum) {
        if (!ppEnum) return E_POINTER;
        *ppEnum = new MockEnumWbemClassObject(BIOS_SPOOFED_SERIAL_NUMBER, m_className.c_str());
        return (*ppEnum) ? S_OK : E_OUTOFMEMORY;
    }
    STDMETHODIMP NextAsync(ULONG, IWbemObjectSink*) { return WBEM_E_NOT_SUPPORTED; }
    STDMETHODIMP Skip(long, ULONG) { return WBEM_E_NOT_SUPPORTED; }
};

// Helper query check for BIOS Serial
inline bool IsBiosSerialQuery(const BSTR strQuery) {
    if (!strQuery) return false;
    bool isSerialQuery = (wcsstr(strQuery, L"SerialNumber") != nullptr);
    bool isBIOSQuery = (wcsstr(strQuery, L"Win32_BIOS") != nullptr);
    return isBIOSQuery && isSerialQuery;
}

// Helper path check for BIOS
inline bool IsBiosPath(const BSTR strPath) {
    if (!strPath) return false;
    return (wcsstr(strPath, L"Win32_BIOS") != nullptr);
}

bool InitializeBiosSerialWMIModule() {
    // Placeholder for any specific initialization for this module if needed in the future.
    // For now, mock objects are created on-demand.
    return true;
}

void CleanupBiosSerialWMIModule() {
    // Placeholder for cleanup if any global resources were allocated by this module.
}

HRESULT Handle_ExecQuery_BiosSerial(IWbemServices*, const BSTR, const BSTR strQuery, long, IWbemContext*, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (IsBiosSerialQuery(strQuery)) {
        *ppEnum = new MockEnumWbemClassObject(BIOS_SPOOFED_SERIAL_NUMBER, L"Win32_BIOS");
        if (!*ppEnum) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: E_OUTOFMEMORY creating MockEnumWbemClassObject in Handle_ExecQuery.\n");
            return E_OUTOFMEMORY;
        }
        handled = true;
        return S_OK;
    }
    return S_OK; // Not an error if not handled, main hook will pass to original
}

HRESULT Handle_GetObject_BiosSerial(IWbemServices*, const BSTR strObjectPath, long, IWbemContext*, IWbemClassObject **ppObject, IWbemCallResult**, bool &handled) {
    handled = false;
    if (IsBiosPath(strObjectPath)) {
        *ppObject = new MockWbemClassObject(BIOS_SPOOFED_SERIAL_NUMBER, L"Win32_BIOS");
        if (!*ppObject) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: E_OUTOFMEMORY creating MockWbemClassObject in Handle_GetObject.\n");
            return E_OUTOFMEMORY;
        }
        // if (ppCallResult) *ppCallResult = NULL; // Client responsibility, usually null for GetObject sync
        handled = true;
        return S_OK;
    }
    return S_OK;
}

HRESULT Handle_CreateInstanceEnum_BiosSerial(IWbemServices*, const BSTR strFilter, long, IWbemContext*, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (IsBiosPath(strFilter)) {
        *ppEnum = new MockEnumWbemClassObject(BIOS_SPOOFED_SERIAL_NUMBER, L"Win32_BIOS");
        if (!*ppEnum) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: E_OUTOFMEMORY creating MockEnumWbemClassObject in Handle_CreateInstanceEnum.\n");
            return E_OUTOFMEMORY;
        }
        handled = true;
        return S_OK;
    }
    return S_OK;
}
