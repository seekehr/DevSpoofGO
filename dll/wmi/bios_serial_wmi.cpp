#include "bios_serial_wmi.h"
#include "../wmi_handler.h" 
#include <atlbase.h> 
#include <atlconv.h> 
#include <comdef.h>
#include <string>     
#include "wmi_classes.h"
#include <ocidl.h> 

const wchar_t* BIOS_SPOOFED_SERIAL_NUMBER = L"F122RCX0031FBNC";

class BiosSerialWbemObject : public MockWbemObject {
private:
    std::wstring m_serialNumber;

public:
    BiosSerialWbemObject(const wchar_t* serial, const wchar_t* className = L"Win32_BIOS")
        : MockWbemObject(className), m_serialNumber(serial) {}

    const std::wstring& GetSerialNumberWstr() const { return m_serialNumber; }

    // Implement pure virtual methods from MockWbemObject
    STDMETHODIMP Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE *pType, long *plFlavor) override {
        if (!wszName) return E_INVALIDARG;
        if (_wcsicmp(wszName, L"SerialNumber") == 0) {
            if (!pVal) { // Client might be querying for type/flavor only
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
        return WBEM_E_NOT_FOUND; // Property not found
    }

    STDMETHODIMP GetNames(LPCWSTR wszQualifierName, long lFlags, VARIANT *pQualifierVal, SAFEARRAY **pNames) override {
        if (!pNames) return E_POINTER;
        SAFEARRAYBOUND bounds; 
        bounds.lLbound = 0; 
        bounds.cElements = 1;
        *pNames = SafeArrayCreate(VT_BSTR, 1, &bounds);
        if (!*pNames) return E_OUTOFMEMORY;

        BSTR propName = SysAllocString(L"SerialNumber");
        if (!propName) {
            SafeArrayDestroy(*pNames);
            *pNames = nullptr;
            return E_OUTOFMEMORY;
        }
        long index = 0;
        HRESULT hr = SafeArrayPutElement(*pNames, &index, propName);
        SysFreeString(propName); // Free the BSTR after putting it in the SAFEARRAY
        
        return SUCCEEDED(hr) ? S_OK : hr;
    }

    STDMETHODIMP Clone(IWbemClassObject **ppCopy) override {
        if (!ppCopy) return E_POINTER;
        // Create a new instance of BiosSerialWbemObject with the same serial and class name
        *ppCopy = new BiosSerialWbemObject(m_serialNumber.c_str(), m_className.c_str());
        return (*ppCopy) ? S_OK : E_OUTOFMEMORY;
    }
};

class BiosSerialEnumWbemObject : public MockEnumWbemObject {
public:
    BiosSerialEnumWbemObject(const wchar_t* serial, const wchar_t* className = L"Win32_BIOS")
        : MockEnumWbemObject(new BiosSerialWbemObject(serial, className), className) {
        // The base class constructor AddRefs the m_mockObject, 
        // and the m_mockObject was just created with new, so its ref count is 1.
        // The base class destructor will Release m_mockObject. So, this is fine.
        if (!m_mockObject) {
             OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: Failed to create BiosSerialWbemObject in BiosSerialEnumWbemObject constructor. E_OUTOFMEMORY.\n");
             // idk how to handle this, maybe throw or set an internal error state
        }
    }

    // Implement pure virtual Clone method
    STDMETHODIMP Clone(IEnumWbemClassObject **ppEnum) override {
        if (!ppEnum) return E_POINTER;

        BiosSerialWbemObject* currentBiosObject = static_cast<BiosSerialWbemObject*>(m_mockObject);
        if (!currentBiosObject) {
            return E_UNEXPECTED; // Should not happen if constructor succeeded
        }
        
        // Clone with the actual serial number of the currently enumerated object
        *ppEnum = new BiosSerialEnumWbemObject(currentBiosObject->GetSerialNumberWstr().c_str(), m_className.c_str());
        return (*ppEnum) ? S_OK : E_OUTOFMEMORY;
    }
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

HRESULT Handle_ExecQuery_BiosSerial(IWbemServices*, const BSTR, const BSTR strQuery, long, IWbemContext*, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (IsBiosSerialQuery(strQuery)) {
        if (!ppEnum) return E_POINTER;
        *ppEnum = new BiosSerialEnumWbemObject(BIOS_SPOOFED_SERIAL_NUMBER);
        if (!*ppEnum) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: Failed to create BiosSerialEnumWbemObject for ExecQuery. E_OUTOFMEMORY.\n");
            return E_OUTOFMEMORY;
        }
        handled = true;
        return S_OK;
    }
    return S_OK; 
}

HRESULT Handle_GetObject_BiosSerial(IWbemServices*, const BSTR strObjectPath, long, IWbemContext*, IWbemClassObject **ppObject, IWbemCallResult **ppCallResult, bool &handled) {
    handled = false;
    if (IsBiosPath(strObjectPath)) {
        if (!ppObject) return E_POINTER;
        *ppObject = new BiosSerialWbemObject(BIOS_SPOOFED_SERIAL_NUMBER);
        if (!*ppObject) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: Failed to create BiosSerialWbemObject for GetObject. E_OUTOFMEMORY.\n");
            return E_OUTOFMEMORY;
        }
        if (ppCallResult) *ppCallResult = nullptr; 
        handled = true;
        return S_OK;
    }
    return S_OK; 
}

HRESULT Handle_CreateInstanceEnum_BiosSerial(IWbemServices*, const BSTR strFilter, long, IWbemContext*, IEnumWbemClassObject **ppEnum, bool &handled) {
    handled = false;
    if (strFilter && _wcsicmp(strFilter, L"Win32_BIOS") == 0) {
        if (!ppEnum) return E_POINTER;
        *ppEnum = new BiosSerialEnumWbemObject(BIOS_SPOOFED_SERIAL_NUMBER);
        if (!*ppEnum) {
            OutputDebugStringW(L"[WMI_BIOS_SERIAL] CRITICAL: Failed to create BiosSerialEnumWbemObject for CreateInstanceEnum. E_OUTOFMEMORY.\n");
            return E_OUTOFMEMORY;
        }
        handled = true;
        return S_OK;
    }
    return S_OK; 
}
