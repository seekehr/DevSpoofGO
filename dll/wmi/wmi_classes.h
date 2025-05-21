#pragma once

#include <windows.h>
// Include objbase.h for DEFINE_GUID and standard IID definitions.
// This should come before other headers that might also try to define them differently.
#include <objbase.h> 

#include <atlbase.h> 
#include <ocidl.h>  
#include <unknwn.h> 
#include <string>    
#include <wbemidl.h> 

#ifndef IID_IClientSecurity
// {0000013D-0000-0000-C000-000000000046}
DEFINE_GUID(IID_IClientSecurity, 0x0000013D, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
#endif

#ifndef IID_IConnectionPointContainer
// {B196B284-BAB4-101A-B69C-00AA00341F07}
DEFINE_GUID(IID_IConnectionPointContainer_LocalDef, 0xB196B284, 0xBAB4, 0x101A, 0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1F, 0x07);
#define MOCK_CPC_TARGET_IID IID_IConnectionPointContainer_LocalDef
#else
#define MOCK_CPC_TARGET_IID IID_IConnectionPointContainer
#endif


class MockClientSecurity : public IClientSecurity {
private:
    ULONG m_cRef;
public:
    MockClientSecurity();
    ~MockClientSecurity();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClientSecurity methods
    STDMETHODIMP QueryBlanket(IUnknown *pProxy, DWORD *pAuthnSvc, DWORD *pAuthzSvc, OLECHAR **pServerPrincipalName, DWORD *pAuthnLevel, DWORD *pImpLevel, void **pAuthInfo, DWORD *pCapabilities) override;
    STDMETHODIMP SetBlanket(IUnknown *pProxy, DWORD AuthnSvc, DWORD AuthzSvc, OLECHAR *pServerPrincipalName, DWORD AuthnLevel, DWORD ImpLevel, void *pAuthInfo, DWORD Capabilities) override;
    STDMETHODIMP CopyProxy(IUnknown *pProxy, IUnknown **ppCopy) override;
};

class MockConnectionPointContainer : public IConnectionPointContainer {
private:
    ULONG m_cRef;
public:
    MockConnectionPointContainer();
    ~MockConnectionPointContainer();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IConnectionPointContainer methods
    STDMETHODIMP EnumConnectionPoints(IEnumConnectionPoints **ppEnum) override;
    STDMETHODIMP FindConnectionPoint(REFIID riid, IConnectionPoint **ppCP) override;
};

// Forward declarations for upcoming abstract base classes
class MockWbemObject;
class MockEnumWbemObject;

class MockWbemObject : public IWbemClassObject {
protected:
    ULONG m_cRef;
    std::wstring m_className;
    MockConnectionPointContainer* m_pCPC; // Reusable component
    IClientSecurity* m_pClientSecurity;   // Reusable component

public:
    MockWbemObject(const wchar_t* className);
    virtual ~MockWbemObject();

    // IUnknown methods - These are virtual in IUnknown and thus can be overridden
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IWbemClassObject methods - These are NOT all virtual in IWbemClassObject
    STDMETHODIMP Put(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE type);
    STDMETHODIMP Delete(LPCWSTR wszName);
    STDMETHODIMP BeginEnumeration(long lEnumFlags);
    STDMETHODIMP Next(long lFlags, BSTR *strName, VARIANT *pVal, CIMTYPE *pType, long *plFlavor);
    STDMETHODIMP EndEnumeration();
    STDMETHODIMP GetPropertyQualifierSet(LPCWSTR wszProperty, IWbemQualifierSet **ppQualSet);
    STDMETHODIMP GetObjectText(long lFlags, BSTR *pstrObjectText);
    STDMETHODIMP SpawnDerivedClass(long lFlags, IWbemClassObject **ppNewClass);
    STDMETHODIMP SpawnInstance(long lFlags, IWbemClassObject **ppNewInstance);
    STDMETHODIMP CompareTo(long lFlags, IWbemClassObject *pCompareTo);
    STDMETHODIMP GetPropertyOrigin(LPCWSTR wszProperty, BSTR *pstrClassName);
    STDMETHODIMP InheritsFrom(LPCWSTR strAncestor);
    STDMETHODIMP GetMethod(LPCWSTR wszName, long lFlags, IWbemClassObject **ppInSignature, IWbemClassObject **ppOutSignature);
    STDMETHODIMP PutMethod(LPCWSTR wszName, long lFlags, IWbemClassObject *pInSignature, IWbemClassObject *pOutSignature);
    STDMETHODIMP DeleteMethod(LPCWSTR wszName);
    STDMETHODIMP BeginMethodEnumeration(long lEnumFlags);
    STDMETHODIMP NextMethod(long lFlags, BSTR *pstrName, IWbemClassObject **ppInSignature, IWbemClassObject **ppOutSignature);
    STDMETHODIMP EndMethodEnumeration();
    STDMETHODIMP GetMethodQualifierSet(LPCWSTR wszMethod, IWbemQualifierSet **ppQualSet);
    STDMETHODIMP GetMethodOrigin(LPCWSTR wszMethod, BSTR *pstrClassName);
    STDMETHODIMP GetQualifierSet(IWbemQualifierSet **ppQualSet);

    // Pure virtual methods to be implemented by derived classes
    // These are specific to our mock implementation and how we want derived classes to behave
    virtual STDMETHODIMP Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE *pType, long *plFlavor) = 0;
    virtual STDMETHODIMP GetNames(LPCWSTR wszQualifierName, long lFlags, VARIANT *pQualifierVal, SAFEARRAY **pNames) = 0;
    virtual STDMETHODIMP Clone(IWbemClassObject **ppCopy) = 0;

    const std::wstring& GetClassNameWstr() const { return m_className; }
};

class MockEnumWbemObject : public IEnumWbemClassObject {
protected:
    ULONG m_cRef;
    bool m_hasReturned;
    MockWbemObject* m_mockObject; // Pointer to the abstract base class 
    std::wstring m_className;     // Class name this enumerator is for
    MockConnectionPointContainer* m_pCPC;
    IClientSecurity* m_pClientSecurity;

public:
    MockEnumWbemObject(MockWbemObject* mockObject, const wchar_t* className);
    virtual ~MockEnumWbemObject();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IEnumWbemClassObject methods
    STDMETHODIMP Reset() override;
    STDMETHODIMP Next(long lTimeout, ULONG uCount, IWbemClassObject **apObjects, ULONG *puReturned) override;
    STDMETHODIMP NextAsync(ULONG uCount, IWbemObjectSink *pSink) override;
    STDMETHODIMP Skip(long lTimeout, ULONG nCount) override;

    // Pure virtual method to be implemented by derived classes for specific cloning behavior
    virtual STDMETHODIMP Clone(IEnumWbemClassObject **ppEnum) = 0;
};

// Forward declarations for WMI specific classes that will use the mocks
// These will be defined in their respective .h files (e.g., bios_serial_wmi.h)
// Example:
// class BiosSerialWbemObject;
// class BiosSerialEnumWbemObject;
