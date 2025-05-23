#include "wmi_bios_serial.h"
#include <atlbase.h> 
#include <atlconv.h> 
#include <comdef.h>
#include <string>     
#include "wmi_utils.h"
#include <ocidl.h> 
#include <wbemidl.h> 

const wchar_t* BIOS_SPOOFED_SERIAL_NUMBER = L"F122RCX0031FBNC";

class BiosSerialWbemObject : public MockWbemObject {
private:
    std::wstring m_serialNumber;
    IWbemClassObject* m_pRealObject; 

public:
    BiosSerialWbemObject(const wchar_t* serial, const wchar_t* className = L"Win32_BIOS", IWbemClassObject* pReal = nullptr)
        : MockWbemObject(className), m_serialNumber(serial), m_pRealObject(pReal) {
        if (m_pRealObject) {
            m_pRealObject->AddRef();
        }
    }

    ~BiosSerialWbemObject() {
        if (m_pRealObject) {
            m_pRealObject->Release();
            m_pRealObject = nullptr;
        }
    }

    const std::wstring& GetSerialNumberWstr() const { return m_serialNumber; }
    IWbemClassObject* GetRealObject() const { return m_pRealObject; }

    STDMETHODIMP Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE *pType, long *plFlavor) override {
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
                return E_OUTOFMEMORY;
            }
            if (pType) *pType = CIM_STRING;
            return S_OK;
        }
        if (m_pRealObject) {
            HRESULT hr_real_get = m_pRealObject->Get(wszName, lFlags, pVal, pType, plFlavor);
            return hr_real_get;
        }
        return WBEM_E_NOT_FOUND; 
    }

    STDMETHODIMP GetNames(LPCWSTR wszQualifierName, long lFlags, VARIANT *pQualifierVal, SAFEARRAY **pNames) override {
        if (!pNames) return E_POINTER;
        if (m_pRealObject) {
            HRESULT hr_real_getnames = m_pRealObject->GetNames(wszQualifierName, lFlags, pQualifierVal, pNames);
            return hr_real_getnames;
        }
        SAFEARRAYBOUND bounds; 
        bounds.lLbound = 0; 
        bounds.cElements = 1;
        *pNames = SafeArrayCreate(VT_BSTR, 1, &bounds);
        if (!*pNames) { return E_OUTOFMEMORY; }
        BSTR propName = SysAllocString(L"SerialNumber");
        if (!propName) {
            SafeArrayDestroy(*pNames); *pNames = nullptr;
            return E_OUTOFMEMORY;
        }
        long index = 0;
        HRESULT hr = SafeArrayPutElement(*pNames, &index, propName);
        SysFreeString(propName); 
        return SUCCEEDED(hr) ? S_OK : hr;
    }

    STDMETHODIMP Clone(IWbemClassObject **ppCopy) override {
        if (!ppCopy) return E_POINTER;
        *ppCopy = new BiosSerialWbemObject(m_serialNumber.c_str(), m_className.c_str(), m_pRealObject);
        return (*ppCopy) ? S_OK : E_OUTOFMEMORY;
    }
};

class BiosSerialEnumWbemObject : public MockEnumWbemObject {
public:
    BiosSerialEnumWbemObject(BiosSerialWbemObject* biosObject, const wchar_t* className = L"Win32_BIOS")
        : MockEnumWbemObject(biosObject, className) { 
        if (!m_mockObject) { 
        }
    }
     ~BiosSerialEnumWbemObject() {
    }

    STDMETHODIMP Clone(IEnumWbemClassObject **ppEnum) override {
        if (!ppEnum) return E_POINTER;
        *ppEnum = nullptr;
        if (!m_mockObject) { 
            return E_UNEXPECTED;
        }
        IWbemClassObject* pClonedBiosWbemObject = nullptr;
        HRESULT hr_clone_obj = m_mockObject->Clone(&pClonedBiosWbemObject); 
                                                        
        if (FAILED(hr_clone_obj) || !pClonedBiosWbemObject) {
            if (pClonedBiosWbemObject) pClonedBiosWbemObject->Release();
            return SUCCEEDED(hr_clone_obj) ? E_FAIL : hr_clone_obj;
        }
        BiosSerialWbemObject* clonedAsBiosSerial = static_cast<BiosSerialWbemObject*>(pClonedBiosWbemObject);
        *ppEnum = new BiosSerialEnumWbemObject(clonedAsBiosSerial, m_className.c_str());
        clonedAsBiosSerial->Release(); 
        if (!*ppEnum) {
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
};

inline bool IsBiosSerialQuery(const BSTR strQuery) {
    if (!strQuery) return false;
    bool isSerialQuery = (wcsstr(strQuery, L"SerialNumber") != nullptr);
    bool isBIOSQuery = (wcsstr(strQuery, L"Win32_BIOS") != nullptr);
    return isBIOSQuery && isSerialQuery;
}

inline bool IsBiosPath(const BSTR strPath) {
    if (!strPath) return false;
    bool isMatch = (wcsstr(strPath, L"Win32_BIOS") != nullptr);
    return isMatch;
}

HRESULT Handle_ExecQuery_BiosSerial(IWbemServices* pProxyServices, const BSTR strQueryLanguage, const BSTR strQuery, long lFlags, IWbemContext* pCtx, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (!strQuery || !ppEnum || wcsstr(strQuery, L"Win32_BIOS") == nullptr) {
        return S_OK; 
    }

    if (!g_real_ExecQuery) {
        if (wcsstr(strQuery, L"SerialNumber") != nullptr && wcsstr(strQuery, L"*") == nullptr) {
            BiosSerialWbemObject* serialOnlyObject = new BiosSerialWbemObject(BIOS_SPOOFED_SERIAL_NUMBER);
            if (!serialOnlyObject) { return E_OUTOFMEMORY; }
            *ppEnum = new BiosSerialEnumWbemObject(serialOnlyObject);
            serialOnlyObject->Release(); 
            if (!*ppEnum) { return E_OUTOFMEMORY; }
            handled = true;
        }
        return S_OK; 
    }

    IEnumWbemClassObject* pRealEnum = nullptr;
    HRESULT hr = g_real_ExecQuery(pProxyServices, strQueryLanguage, strQuery, lFlags, pCtx, &pRealEnum);

    if (FAILED(hr) || !pRealEnum) {
        if (pRealEnum) pRealEnum->Release();
        return SUCCEEDED(hr) ? S_OK : hr;
    }

    IWbemClassObject* pRealBiosObjectRaw = nullptr;
    ULONG uReturned = 0;
    hr = pRealEnum->Next(WBEM_INFINITE, 1, &pRealBiosObjectRaw, &uReturned);
    pRealEnum->Release(); 

    if (FAILED(hr) || uReturned == 0 || !pRealBiosObjectRaw) {
        if (pRealBiosObjectRaw) pRealBiosObjectRaw->Release();
        *ppEnum = new BiosSerialEnumWbemObject(nullptr, L"Win32_BIOS"); 
        if (!*ppEnum) { return E_OUTOFMEMORY; }
        handled = true;
        return S_OK;
    }

    BiosSerialWbemObject* wrapperObject = new BiosSerialWbemObject(BIOS_SPOOFED_SERIAL_NUMBER, L"Win32_BIOS", pRealBiosObjectRaw);
    pRealBiosObjectRaw->Release(); 

    if (!wrapperObject) { return E_OUTOFMEMORY; }

    *ppEnum = new BiosSerialEnumWbemObject(wrapperObject);
    wrapperObject->Release(); 

    if (!*ppEnum) { return E_OUTOFMEMORY; }
    handled = true;
    return S_OK;
}

HRESULT Handle_GetObject_BiosSerial(IWbemServices* pProxyServices, const BSTR strObjectPath, long lFlags, IWbemContext* pCtx, IWbemClassObject **ppObject, IWbemCallResult **ppCallResult, bool &handled) {
    handled = false;
    if (!strObjectPath || !ppObject || !IsBiosPath(strObjectPath)) {
        return S_OK; 
    }

    if (!g_real_GetObject) {
        return S_OK; 
    }

    IWbemClassObject* pRealBiosObject = nullptr;
    HRESULT hr = g_real_GetObject(pProxyServices, strObjectPath, lFlags, pCtx, &pRealBiosObject, ppCallResult);

    if (FAILED(hr) || !pRealBiosObject) {
        if (pRealBiosObject) pRealBiosObject->Release();
        return SUCCEEDED(hr) ? S_OK : hr; 
    }

    *ppObject = new BiosSerialWbemObject(BIOS_SPOOFED_SERIAL_NUMBER, L"Win32_BIOS", pRealBiosObject);
    pRealBiosObject->Release(); 

    if (!*ppObject) { 
        return E_OUTOFMEMORY; 
    }
    
    handled = true;
    return S_OK;
}

HRESULT Handle_CreateInstanceEnum_BiosSerial(IWbemServices* pProxyServices, const BSTR strFilter, long lFlags, IWbemContext* pCtx, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (!strFilter || !ppEnum || _wcsicmp(strFilter, L"Win32_BIOS") != 0) {
        return S_OK; 
    }

    if (!g_real_CreateInstanceEnum) {
        BiosSerialWbemObject* serialOnlyObject = new BiosSerialWbemObject(BIOS_SPOOFED_SERIAL_NUMBER);
        if (!serialOnlyObject) { return E_OUTOFMEMORY; }
        *ppEnum = new BiosSerialEnumWbemObject(serialOnlyObject);
        serialOnlyObject->Release(); 
        if (!*ppEnum) { return E_OUTOFMEMORY; }
        handled = true;
        return S_OK;
    }

    IEnumWbemClassObject* pRealEnum = nullptr;
    HRESULT hr = g_real_CreateInstanceEnum(pProxyServices, strFilter, lFlags, pCtx, &pRealEnum);

    if (FAILED(hr) || !pRealEnum) {
        if (pRealEnum) pRealEnum->Release();
        return SUCCEEDED(hr) ? S_OK : hr; 
    }

    IWbemClassObject* pRealBiosObjectRaw = nullptr;
    ULONG uReturned = 0;
    hr = pRealEnum->Next(WBEM_INFINITE, 1, &pRealBiosObjectRaw, &uReturned);
    pRealEnum->Release(); 

    if (FAILED(hr) || uReturned == 0 || !pRealBiosObjectRaw) {
        if (pRealBiosObjectRaw) pRealBiosObjectRaw->Release();
        *ppEnum = new BiosSerialEnumWbemObject(nullptr, L"Win32_BIOS"); 
        if (!*ppEnum) { return E_OUTOFMEMORY; }
        handled = true;
        return S_OK;
    }
    
    BiosSerialWbemObject* wrapperObject = new BiosSerialWbemObject(BIOS_SPOOFED_SERIAL_NUMBER, L"Win32_BIOS", pRealBiosObjectRaw);
    pRealBiosObjectRaw->Release(); 

    if (!wrapperObject) { return E_OUTOFMEMORY; }

    *ppEnum = new BiosSerialEnumWbemObject(wrapperObject);
    wrapperObject->Release(); 

    if (!*ppEnum) { return E_OUTOFMEMORY; }
    handled = true;
    return S_OK;
}
