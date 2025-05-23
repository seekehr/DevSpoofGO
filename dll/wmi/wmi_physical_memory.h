#pragma once

#include <windows.h>
#include <wbemidl.h> // For IEnumWbemClassObject, IWbemClassObject, IWbemContext, IWbemServices, IWbemCallResult

// Forward declaration for the PhysicalMemory-specific WMI object class
class PhysicalMemoryWbemObject;

// Forward declaration for the enumerator for PhysicalMemory objects
class PhysicalMemoryEnumWbemObject;

// Handler functions for specific WMI methods related to Win32_PhysicalMemory.
// They attempt to handle the call; if handled, they set 'handled' to true and return the appropriate HRESULT.
// If not handled, 'handled' is set to false.
HRESULT Handle_ExecQuery_PhysicalMemory(
    IWbemServices *pThis,
    const BSTR strQueryLanguage,
    const BSTR strQuery,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum,
    bool &handled
);

HRESULT Handle_GetObject_PhysicalMemory(
    IWbemServices *pThis,
    const BSTR strObjectPath,
    long lFlags,
    IWbemContext *pCtx,
    IWbemClassObject **ppObject,
    IWbemCallResult **ppCallResult,
    bool &handled
);

HRESULT Handle_CreateInstanceEnum_PhysicalMemory(
    IWbemServices *pThis,
    const BSTR strFilter,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum,
    bool &handled
);
