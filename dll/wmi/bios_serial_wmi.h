#pragma once

#include <windows.h>
#include <wbemidl.h> // For IEnumWbemClassObject, IWbemClassObject, IWbemContext, IWbemServices, IWbemCallResult

// Forward declarations for mock classes that will be defined in bios_serial_wmi.cpp
class MockWbemClassObject;
class MockEnumWbemClassObject;
class MockClientSecurity;
class MockConnectionPointContainer;

// Initialization and cleanup functions for BIOS serial specific WMI module
bool InitializeBiosSerialWMIModule();
void CleanupBiosSerialWMIModule();

// Handler functions for specific WMI methods.
// They attempt to handle the call; if handled, they set 'handled' to true and return the appropriate HRESULT.
// If not handled, 'handled' is set to false.
HRESULT Handle_ExecQuery_BiosSerial(
    IWbemServices *pThis,
    const BSTR strQueryLanguage,
    const BSTR strQuery,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum,
    bool &handled
);

HRESULT Handle_GetObject_BiosSerial(
    IWbemServices *pThis,
    const BSTR strObjectPath,
    long lFlags,
    IWbemContext *pCtx,
    IWbemClassObject **ppObject,
    IWbemCallResult **ppCallResult,
    bool &handled
);

HRESULT Handle_CreateInstanceEnum_BiosSerial(
    IWbemServices *pThis,
    const BSTR strFilter,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum,
    bool &handled
);
