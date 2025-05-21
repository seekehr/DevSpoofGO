#include <windows.h>     // Should be first for COM
#include <objbase.h>     // For CoInitialize, CoCreateInstance, and other COM basics
#include <initguid.h>    // For DEFINE_GUID to actually define GUIDs

#include "wmi_classes.h" // Our class declarations

// --- MockClientSecurity Implementation ---
MockClientSecurity::MockClientSecurity() : m_cRef(1) {}

MockClientSecurity::~MockClientSecurity() {}

STDMETHODIMP MockClientSecurity::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClientSecurity) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MockClientSecurity::AddRef() { return InterlockedIncrement(&m_cRef); }

STDMETHODIMP_(ULONG) MockClientSecurity::Release() {
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

STDMETHODIMP MockClientSecurity::QueryBlanket(IUnknown *pProxy, DWORD *pAuthnSvc, DWORD *pAuthzSvc, OLECHAR **pServerPrincipalName, DWORD *pAuthnLevel, DWORD *pImpLevel, void **pAuthInfo, DWORD *pCapabilities) {
    if (pAuthnSvc) *pAuthnSvc = RPC_C_AUTHN_WINNT;
    if (pAuthzSvc) *pAuthzSvc = RPC_C_AUTHZ_NONE;
    if (pServerPrincipalName) *pServerPrincipalName = nullptr; 
    if (pAuthnLevel) *pAuthnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY; 
    if (pImpLevel) *pImpLevel = RPC_C_IMP_LEVEL_IMPERSONATE;
    if (pAuthInfo) *pAuthInfo = nullptr; 
    if (pCapabilities) *pCapabilities = EOAC_NONE;
    return S_OK;
}

STDMETHODIMP MockClientSecurity::SetBlanket(IUnknown *pProxy, DWORD AuthnSvc, DWORD AuthzSvc, OLECHAR *pServerPrincipalName, DWORD AuthnLevel, DWORD ImpLevel, void *pAuthInfo, DWORD Capabilities) {
    return S_OK;
}

STDMETHODIMP MockClientSecurity::CopyProxy(IUnknown *pProxy, IUnknown **ppCopy) {
    if (!ppCopy) return E_POINTER;
    *ppCopy = nullptr;
    return E_NOTIMPL;
}

// --- MockConnectionPointContainer Implementation ---
MockConnectionPointContainer::MockConnectionPointContainer() : m_cRef(1) {}

MockConnectionPointContainer::~MockConnectionPointContainer() {}

STDMETHODIMP MockConnectionPointContainer::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == MOCK_CPC_TARGET_IID) {
        *ppvObject = this;
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) MockConnectionPointContainer::AddRef() { return InterlockedIncrement(&m_cRef); }

STDMETHODIMP_(ULONG) MockConnectionPointContainer::Release() {
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

STDMETHODIMP MockConnectionPointContainer::EnumConnectionPoints(IEnumConnectionPoints **ppEnum) {
    if (ppEnum) *ppEnum = NULL;
    return E_NOTIMPL;
}

STDMETHODIMP MockConnectionPointContainer::FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP) {
    if (ppCP) *ppCP = NULL;
    return E_NOTIMPL;
}

// --- MockWbemObject Implementation ---
MockWbemObject::MockWbemObject(const wchar_t* className)
    : m_cRef(1), m_className(className),
      m_pCPC(new MockConnectionPointContainer()), 
      m_pClientSecurity(new MockClientSecurity()) {}

MockWbemObject::~MockWbemObject() {
    if (m_pCPC) m_pCPC->Release();
    if (m_pClientSecurity) m_pClientSecurity->Release();
}

STDMETHODIMP MockWbemObject::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;

    if (riid == IID_IUnknown || riid == IID_IWbemClassObject) {
        *ppvObject = static_cast<IWbemClassObject*>(this);
        AddRef();
        return S_OK;
    } else if (riid == MOCK_CPC_TARGET_IID) {
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

STDMETHODIMP_(ULONG) MockWbemObject::AddRef() { 
    return InterlockedIncrement(&m_cRef); 
}

STDMETHODIMP_(ULONG) MockWbemObject::Release() {
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

// Default implementations for IWbemClassObject methods (mostly E_NOTIMPL)
STDMETHODIMP MockWbemObject::Put(LPCWSTR, long, VARIANT*, CIMTYPE) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::Delete(LPCWSTR) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::BeginEnumeration(long) { return WBEM_S_NO_ERROR; } // Often expected to succeed
STDMETHODIMP MockWbemObject::Next(long, BSTR*, VARIANT*, CIMTYPE*, long*) { return WBEM_S_NO_MORE_DATA; } // No properties by default
STDMETHODIMP MockWbemObject::EndEnumeration() { return WBEM_S_NO_ERROR; } // Often expected to succeed
STDMETHODIMP MockWbemObject::GetPropertyQualifierSet(LPCWSTR, IWbemQualifierSet**) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::GetObjectText(long, BSTR*) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::SpawnDerivedClass(long, IWbemClassObject**) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::SpawnInstance(long, IWbemClassObject**) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::CompareTo(long, IWbemClassObject*) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::GetPropertyOrigin(LPCWSTR, BSTR*) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::InheritsFrom(LPCWSTR) { return E_NOTIMPL; } // Could be WBEM_S_FALSE if not supporting inheritance
STDMETHODIMP MockWbemObject::GetMethod(LPCWSTR, long, IWbemClassObject**, IWbemClassObject**) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::PutMethod(LPCWSTR, long, IWbemClassObject*, IWbemClassObject*) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::DeleteMethod(LPCWSTR) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::BeginMethodEnumeration(long) { return WBEM_S_NO_ERROR; } // Often expected to succeed
STDMETHODIMP MockWbemObject::NextMethod(long, BSTR*, IWbemClassObject**, IWbemClassObject**) { return WBEM_S_NO_MORE_DATA; } // No methods by default
STDMETHODIMP MockWbemObject::EndMethodEnumeration() { return S_OK; }
STDMETHODIMP MockWbemObject::GetMethodQualifierSet(LPCWSTR, IWbemQualifierSet**) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::GetMethodOrigin(LPCWSTR, BSTR*) { return E_NOTIMPL; }
STDMETHODIMP MockWbemObject::GetQualifierSet(IWbemQualifierSet**) { return E_NOTIMPL; }

// --- MockEnumWbemObject Implementation ---
MockEnumWbemObject::MockEnumWbemObject(MockWbemObject* mockObject, const wchar_t* className)
    : m_cRef(1), m_hasReturned(false), m_mockObject(mockObject), m_className(className),
      m_pCPC(new MockConnectionPointContainer()),
      m_pClientSecurity(new MockClientSecurity()) {
    if (m_mockObject) {
        m_mockObject->AddRef(); // The enumerator now holds a reference to the object it enumerates
    }
}

MockEnumWbemObject::~MockEnumWbemObject() {
    if (m_mockObject) m_mockObject->Release();
    if (m_pCPC) m_pCPC->Release();
    if (m_pClientSecurity) m_pClientSecurity->Release();
}

STDMETHODIMP MockEnumWbemObject::QueryInterface(REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown || riid == IID_IEnumWbemClassObject) {
        *ppvObject = static_cast<IEnumWbemClassObject*>(this);
        AddRef();
        return S_OK;
    } else if (riid == MOCK_CPC_TARGET_IID) {
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

STDMETHODIMP_(ULONG) MockEnumWbemObject::AddRef() { 
    return InterlockedIncrement(&m_cRef); 
}

STDMETHODIMP_(ULONG) MockEnumWbemObject::Release() {
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) delete this;
    return cRef;
}

STDMETHODIMP MockEnumWbemObject::Reset() { 
    m_hasReturned = false; 
    return S_OK; 
}

STDMETHODIMP MockEnumWbemObject::Next(long lTimeout, ULONG uCount, IWbemClassObject **apObjects, ULONG *puReturned) {
    if (!apObjects || !puReturned) return WBEM_E_INVALID_PARAMETER;
    *puReturned = 0;
    if (uCount == 0) return WBEM_S_NO_ERROR; // Nothing to return

    // Initialize output array
    for (ULONG i = 0; i < uCount; i++) {
        apObjects[i] = nullptr;
    }

    if (m_hasReturned || !m_mockObject) {
        return WBEM_S_FALSE; // No more objects or no object to enumerate
    }

    // This mock enumerator only returns one object
    apObjects[0] = m_mockObject;
    m_mockObject->AddRef(); // AddRef the object being returned
    *puReturned = 1;
    m_hasReturned = true;
    return WBEM_S_NO_ERROR;
}

STDMETHODIMP MockEnumWbemObject::NextAsync(ULONG uCount, IWbemObjectSink *pSink) { 
    // do later ;D
    return WBEM_E_NOT_SUPPORTED; 
}

STDMETHODIMP MockEnumWbemObject::Skip(long lTimeout, ULONG nCount) { 
    // Skipping is also generally not implemented for simple, single-object enumerators.
    // If nCount is 0, it's a no-op. If nCount is 1 and we haven't returned, we can skip.
    if (nCount == 1 && !m_hasReturned && m_mockObject) {
        m_hasReturned = true;
        return WBEM_S_NO_ERROR;
    }
    return WBEM_E_NOT_SUPPORTED; // Or WBEM_S_FALSE if already past the single object
}
