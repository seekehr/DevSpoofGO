#include "wmi_physical_memory.h"
#include <atlbase.h> 
#include <atlconv.h> 
#include <comdef.h>
#include <string>     
#include "wmi_utils.h"
#include <ocidl.h> 
#include <wbemidl.h> 

const wchar_t* PHYSICAL_MEMORY_SPOOFED_SERIAL_NUMBER = L"01300000";
const wchar_t* PHYSICAL_MEMORY_SPOOFED_PART_NUMBER = L"HMA82GU6CJR8N-VK";

class PhysicalMemoryWbemObject : public MockWbemObject {
private:
    std::wstring m_serialNumber;
    std::wstring m_partNumber;
    IWbemClassObject* m_pRealObject; 

public:
    PhysicalMemoryWbemObject(const wchar_t* serial, const wchar_t* partNumber, const wchar_t* className = L"Win32_PhysicalMemory", IWbemClassObject* pReal = nullptr)
        : MockWbemObject(className), m_serialNumber(serial), m_partNumber(partNumber), m_pRealObject(pReal) {
        if (m_pRealObject) {
            m_pRealObject->AddRef();
        }
    }

    ~PhysicalMemoryWbemObject() {
        if (m_pRealObject) {
            m_pRealObject->Release();
            m_pRealObject = nullptr;
        }
    }

    const std::wstring& GetSerialNumberWstr() const { return m_serialNumber; }
    const std::wstring& GetPartNumberWstr() const { return m_partNumber; }
    IWbemClassObject* GetRealObject() const { return m_pRealObject; }

    STDMETHODIMP Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE *pType, long *plFlavor) override {
        if (!wszName) return E_INVALIDARG;
        
        // Handle SerialNumber property
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
        
        // Handle PartNumber property
        if (_wcsicmp(wszName, L"PartNumber") == 0) {
            if (!pVal) { 
                 if (pType) *pType = CIM_STRING;
                 if (plFlavor) *plFlavor = WBEM_FLAVOR_ORIGIN_PROPAGATED;
                 return S_OK;
            }
            VariantInit(pVal);
            pVal->vt = VT_BSTR;
            pVal->bstrVal = SysAllocString(m_partNumber.c_str());
            if (!pVal->bstrVal) {
                return E_OUTOFMEMORY;
            }
            if (pType) *pType = CIM_STRING;
            return S_OK;
        }
        
        // For all other properties, delegate to the real object if available
        if (m_pRealObject) {
            HRESULT hr_real_get = m_pRealObject->Get(wszName, lFlags, pVal, pType, plFlavor);
            return hr_real_get;
        }
        
        return WBEM_E_NOT_FOUND; 
    }

    STDMETHODIMP GetNames(LPCWSTR wszQualifierName, long lFlags, VARIANT *pQualifierVal, SAFEARRAY **pNames) override {
        if (!pNames) return E_POINTER;
        
        // If we have a real object, delegate to it
        if (m_pRealObject) {
            HRESULT hr_real_getnames = m_pRealObject->GetNames(wszQualifierName, lFlags, pQualifierVal, pNames);
            return hr_real_getnames;
        }
        
        // Otherwise, create a minimal array with our spoofed properties
        SAFEARRAYBOUND bounds; 
        bounds.lLbound = 0; 
        bounds.cElements = 2; // Two properties: SerialNumber and PartNumber
        *pNames = SafeArrayCreate(VT_BSTR, 1, &bounds);
        if (!*pNames) { return E_OUTOFMEMORY; }
        
        // Add SerialNumber
        BSTR propName1 = SysAllocString(L"SerialNumber");
        if (!propName1) {
            SafeArrayDestroy(*pNames); *pNames = nullptr;
            return E_OUTOFMEMORY;
        }
        long index = 0;
        HRESULT hr = SafeArrayPutElement(*pNames, &index, propName1);
        SysFreeString(propName1);
        if (FAILED(hr)) {
            SafeArrayDestroy(*pNames); *pNames = nullptr;
            return hr;
        }
        
        // Add PartNumber
        BSTR propName2 = SysAllocString(L"PartNumber");
        if (!propName2) {
            SafeArrayDestroy(*pNames); *pNames = nullptr;
            return E_OUTOFMEMORY;
        }
        index = 1;
        hr = SafeArrayPutElement(*pNames, &index, propName2);
        SysFreeString(propName2);
        
        return SUCCEEDED(hr) ? S_OK : hr;
    }

    STDMETHODIMP Clone(IWbemClassObject **ppCopy) override {
        if (!ppCopy) return E_POINTER;
        *ppCopy = new PhysicalMemoryWbemObject(m_serialNumber.c_str(), m_partNumber.c_str(), m_className.c_str(), m_pRealObject);
        return (*ppCopy) ? S_OK : E_OUTOFMEMORY;
    }
};

class PhysicalMemoryEnumWbemObject : public MockEnumWbemObject {
public:
    PhysicalMemoryEnumWbemObject(PhysicalMemoryWbemObject* physicalMemoryObject, const wchar_t* className = L"Win32_PhysicalMemory")
        : MockEnumWbemObject(physicalMemoryObject, className) { }
        
    ~PhysicalMemoryEnumWbemObject() { }

    STDMETHODIMP Clone(IEnumWbemClassObject **ppEnum) override {
        if (!ppEnum) return E_POINTER;
        *ppEnum = nullptr;
        
        if (!m_mockObject) { 
            return E_UNEXPECTED;
        }
        
        IWbemClassObject* pClonedPhysicalMemoryWbemObject = nullptr;
        HRESULT hr_clone_obj = m_mockObject->Clone(&pClonedPhysicalMemoryWbemObject);
                                                        
        if (FAILED(hr_clone_obj) || !pClonedPhysicalMemoryWbemObject) {
            if (pClonedPhysicalMemoryWbemObject) pClonedPhysicalMemoryWbemObject->Release();
            return SUCCEEDED(hr_clone_obj) ? E_FAIL : hr_clone_obj;
        }
        
        PhysicalMemoryWbemObject* clonedAsPhysicalMemory = static_cast<PhysicalMemoryWbemObject*>(pClonedPhysicalMemoryWbemObject);
        *ppEnum = new PhysicalMemoryEnumWbemObject(clonedAsPhysicalMemory, m_className.c_str());
        clonedAsPhysicalMemory->Release(); 
        
        if (!*ppEnum) {
            return E_OUTOFMEMORY;
        }
        
        return S_OK;
    }
};

// Helper functions to check if a query or path is related to Win32_PhysicalMemory
inline bool IsPhysicalMemoryQuery(const BSTR strQuery) {
    if (!strQuery) return false;
    bool isPhysicalMemoryQuery = (wcsstr(strQuery, L"Win32_PhysicalMemory") != nullptr);
    return isPhysicalMemoryQuery;
}

inline bool IsPhysicalMemoryPath(const BSTR strPath) {
    if (!strPath) return false;
    bool isMatch = (wcsstr(strPath, L"Win32_PhysicalMemory") != nullptr);
    return isMatch;
}

HRESULT Handle_ExecQuery_PhysicalMemory(IWbemServices* pProxyServices, const BSTR strQueryLanguage, const BSTR strQuery, long lFlags, IWbemContext* pCtx, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (!strQuery || !ppEnum || !IsPhysicalMemoryQuery(strQuery)) {
        return S_OK; 
    }

    // If the original ExecQuery function isn't available, create a mock object with just our spoofed values
    if (!g_real_ExecQuery) {
        PhysicalMemoryWbemObject* physicalMemoryObject = new PhysicalMemoryWbemObject(PHYSICAL_MEMORY_SPOOFED_SERIAL_NUMBER, PHYSICAL_MEMORY_SPOOFED_PART_NUMBER);
        if (!physicalMemoryObject) { return E_OUTOFMEMORY; }
        *ppEnum = new PhysicalMemoryEnumWbemObject(physicalMemoryObject);
        physicalMemoryObject->Release(); 
        if (!*ppEnum) { return E_OUTOFMEMORY; }
        handled = true;
        return S_OK;
    }

    // Otherwise, call the original function and wrap its result with our spoofed values
    IEnumWbemClassObject* pRealEnum = nullptr;
    HRESULT hr = g_real_ExecQuery(pProxyServices, strQueryLanguage, strQuery, lFlags, pCtx, &pRealEnum);

    if (FAILED(hr) || !pRealEnum) {
        if (pRealEnum) pRealEnum->Release();
        return SUCCEEDED(hr) ? S_OK : hr;
    }

    IWbemClassObject* pRealPhysicalMemoryObjectRaw = nullptr;
    ULONG uReturned = 0;
    hr = pRealEnum->Next(WBEM_INFINITE, 1, &pRealPhysicalMemoryObjectRaw, &uReturned);
    pRealEnum->Release(); 

    if (FAILED(hr) || uReturned == 0 || !pRealPhysicalMemoryObjectRaw) {
        if (pRealPhysicalMemoryObjectRaw) pRealPhysicalMemoryObjectRaw->Release();
        *ppEnum = new PhysicalMemoryEnumWbemObject(nullptr, L"Win32_PhysicalMemory"); 
        if (!*ppEnum) { return E_OUTOFMEMORY; }
        handled = true;
        return S_OK;
    }

    PhysicalMemoryWbemObject* wrapperObject = new PhysicalMemoryWbemObject(
        PHYSICAL_MEMORY_SPOOFED_SERIAL_NUMBER, 
        PHYSICAL_MEMORY_SPOOFED_PART_NUMBER, 
        L"Win32_PhysicalMemory", 
        pRealPhysicalMemoryObjectRaw
    );
    pRealPhysicalMemoryObjectRaw->Release(); 

    if (!wrapperObject) { return E_OUTOFMEMORY; }

    *ppEnum = new PhysicalMemoryEnumWbemObject(wrapperObject);
    wrapperObject->Release(); 

    if (!*ppEnum) { return E_OUTOFMEMORY; }
    handled = true;
    return S_OK;
}

HRESULT Handle_GetObject_PhysicalMemory(IWbemServices* pProxyServices, const BSTR strObjectPath, long lFlags, IWbemContext* pCtx, IWbemClassObject **ppObject, IWbemCallResult **ppCallResult, bool &handled) {
    handled = false;
    if (!strObjectPath || !ppObject || !IsPhysicalMemoryPath(strObjectPath)) {
        return S_OK; 
    }

    if (!g_real_GetObject) {
        return S_OK; 
    }

    IWbemClassObject* pRealPhysicalMemoryObject = nullptr;
    HRESULT hr = g_real_GetObject(pProxyServices, strObjectPath, lFlags, pCtx, &pRealPhysicalMemoryObject, ppCallResult);

    if (FAILED(hr) || !pRealPhysicalMemoryObject) {
        if (pRealPhysicalMemoryObject) pRealPhysicalMemoryObject->Release();
        return SUCCEEDED(hr) ? S_OK : hr; 
    }

    *ppObject = new PhysicalMemoryWbemObject(
        PHYSICAL_MEMORY_SPOOFED_SERIAL_NUMBER, 
        PHYSICAL_MEMORY_SPOOFED_PART_NUMBER, 
        L"Win32_PhysicalMemory", 
        pRealPhysicalMemoryObject
    );
    pRealPhysicalMemoryObject->Release(); 

    if (!*ppObject) { 
        return E_OUTOFMEMORY; 
    }
    
    handled = true;
    return S_OK;
}

HRESULT Handle_CreateInstanceEnum_PhysicalMemory(IWbemServices* pProxyServices, const BSTR strFilter, long lFlags, IWbemContext* pCtx, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (!strFilter || !ppEnum || _wcsicmp(strFilter, L"Win32_PhysicalMemory") != 0) {
        return S_OK; 
    }

    if (!g_real_CreateInstanceEnum) {
        PhysicalMemoryWbemObject* physicalMemoryObject = new PhysicalMemoryWbemObject(PHYSICAL_MEMORY_SPOOFED_SERIAL_NUMBER, PHYSICAL_MEMORY_SPOOFED_PART_NUMBER);
        if (!physicalMemoryObject) { return E_OUTOFMEMORY; }
        *ppEnum = new PhysicalMemoryEnumWbemObject(physicalMemoryObject);
        physicalMemoryObject->Release(); 
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

    IWbemClassObject* pRealPhysicalMemoryObjectRaw = nullptr;
    ULONG uReturned = 0;
    hr = pRealEnum->Next(WBEM_INFINITE, 1, &pRealPhysicalMemoryObjectRaw, &uReturned);
    pRealEnum->Release(); 

    if (FAILED(hr) || uReturned == 0 || !pRealPhysicalMemoryObjectRaw) {
        if (pRealPhysicalMemoryObjectRaw) pRealPhysicalMemoryObjectRaw->Release();
        *ppEnum = new PhysicalMemoryEnumWbemObject(nullptr, L"Win32_PhysicalMemory"); 
        if (!*ppEnum) { return E_OUTOFMEMORY; }
        handled = true;
        return S_OK;
    }
    
    PhysicalMemoryWbemObject* wrapperObject = new PhysicalMemoryWbemObject(
        PHYSICAL_MEMORY_SPOOFED_SERIAL_NUMBER, 
        PHYSICAL_MEMORY_SPOOFED_PART_NUMBER, 
        L"Win32_PhysicalMemory", 
        pRealPhysicalMemoryObjectRaw
    );
    pRealPhysicalMemoryObjectRaw->Release(); 

    if (!wrapperObject) { return E_OUTOFMEMORY; }

    *ppEnum = new PhysicalMemoryEnumWbemObject(wrapperObject);
    wrapperObject->Release(); 

    if (!*ppEnum) { return E_OUTOFMEMORY; }
    handled = true;
    return S_OK;
}
