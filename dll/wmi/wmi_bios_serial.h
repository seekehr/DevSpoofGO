#pragma once

#include <windows.h>
#include <wbemidl.h> // For IEnumWbemClassObject, IWbemClassObject, IWbemContext, IWbemServices, IWbemCallResult
// wmi_classes.h will be included by .cpp files that need the full definitions
// For this header, forward declarations are sufficient if only pointers/references are used.

// Forward declaration for the BIOS-specific WMI object class
class BiosSerialWbemObject; // Renamed from MockWbemClassObject and now specific

// Forward declaration for the enumerator, which might still be generic or become specific
class BiosSerialEnumWbemObject; // Renamed from MockEnumWbemClassObject and now specific

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
