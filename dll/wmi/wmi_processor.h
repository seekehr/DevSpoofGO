#pragma once

#include <windows.h>
#include <wbemidl.h> // For IEnumWbemClassObject, IWbemClassObject, IWbemContext, IWbemServices, IWbemCallResult

// Forward declaration for the Processor-specific WMI object class
class ProcessorWbemObject;

// Forward declaration for the enumerator for Processor objects
class ProcessorEnumWbemObject;

// Handler functions for specific WMI methods related to Win32_Processor.
// They attempt to handle the call; if handled, they set 'handled' to true and return the appropriate HRESULT.
// If not handled, 'handled' is set to false.
HRESULT Handle_ExecQuery_Processor(
    IWbemServices *pThis,
    const BSTR strQueryLanguage,
    const BSTR strQuery,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum,
    bool &handled
);

HRESULT Handle_GetObject_Processor(
    IWbemServices *pThis,
    const BSTR strObjectPath,
    long lFlags,
    IWbemContext *pCtx,
    IWbemClassObject **ppObject,
    IWbemCallResult **ppCallResult,
    bool &handled
);

HRESULT Handle_CreateInstanceEnum_Processor(
    IWbemServices *pThis,
    const BSTR strFilter,
    long lFlags,
    IWbemContext *pCtx,
    IEnumWbemClassObject **ppEnum,
    bool &handled
);
