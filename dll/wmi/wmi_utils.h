#pragma once

#include <windows.h>

#include <objbase.h> 

#include <atlbase.h> 
#include <ocidl.h>  
#include <unknwn.h> 
#include <string>    
#include <wbemidl.h> 

// Typedefs for original WMI function pointers
typedef HRESULT (STDMETHODCALLTYPE *RealExecQueryType)(
    IWbemServices*, const BSTR, const BSTR, long, IWbemContext*, IEnumWbemClassObject**);
typedef HRESULT (STDMETHODCALLTYPE *RealGetObjectType)(
    IWbemServices*, const BSTR, long, IWbemContext*, IWbemClassObject**, IWbemCallResult**);
typedef HRESULT (STDMETHODCALLTYPE *RealCreateInstanceEnumType)(
    IWbemServices*, const BSTR, long, IWbemContext*, IEnumWbemClassObject**);

// Extern declarations for global original WMI function pointers
// These are defined in wmi_handler.cpp
extern RealExecQueryType g_real_ExecQuery;
extern RealGetObjectType g_real_GetObject;
extern RealCreateInstanceEnumType g_real_CreateInstanceEnum;

#ifndef IID_IClientSecurity
DEFINE_GUID(IID_IClientSecurity, 0x0000013D, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
#endif

#ifndef IID_IConnectionPointContainer
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

class MockWbemObject;
class MockEnumWbemObject;

class MockWbemObject : public IWbemClassObject {
protected:
    ULONG m_cRef;
    std::wstring m_className;
    MockConnectionPointContainer* m_pCPC; 
    IClientSecurity* m_pClientSecurity;   

public:
    MockWbemObject(const wchar_t* className);
    virtual ~MockWbemObject();

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

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

    virtual STDMETHODIMP Get(LPCWSTR wszName, long lFlags, VARIANT *pVal, CIMTYPE *pType, long *plFlavor) = 0;
    virtual STDMETHODIMP GetNames(LPCWSTR wszQualifierName, long lFlags, VARIANT *pQualifierVal, SAFEARRAY **pNames) = 0;
    virtual STDMETHODIMP Clone(IWbemClassObject **ppCopy) = 0;

    const std::wstring& GetClassNameWstr() const { return m_className; }
};

class MockEnumWbemObject : public IEnumWbemClassObject {
protected:
    ULONG m_cRef;
    bool m_hasReturned;
    MockWbemObject* m_mockObject;  
    std::wstring m_className;     
    MockConnectionPointContainer* m_pCPC;
    IClientSecurity* m_pClientSecurity;

public:
    MockEnumWbemObject(MockWbemObject* mockObject, const wchar_t* className);
    virtual ~MockEnumWbemObject();

    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP Reset() override;
    STDMETHODIMP Next(long lTimeout, ULONG uCount, IWbemClassObject **apObjects, ULONG *puReturned) override;
    STDMETHODIMP NextAsync(ULONG uCount, IWbemObjectSink *pSink) override;
    STDMETHODIMP Skip(long lTimeout, ULONG nCount) override;

    virtual STDMETHODIMP Clone(IEnumWbemClassObject **ppEnum) = 0;
};
