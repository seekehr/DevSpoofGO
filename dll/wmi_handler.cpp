// Trampoline for the original IUnknown::AddRef
pfnAddRef True_AddRef = NULL;

// Detour function for IUnknown::AddRef
ULONG STDMETHODCALLTYPE Mine_AddRef(IUnknown* This)
{
    LOG(INFO) << "Mine_AddRef called on IUnknown instance: " << This;
    return True_AddRef(This);
}

// Detour function for CoCreateInstance
HRESULT WINAPI Mine_CoCreateInstance(
    // ... existing code ...

    // Check if the object creation was successful
    if (SUCCEEDED(hr))
    {
        LOG(INFO) << "CoCreateInstance succeeded for CLSID: " << clsid_str << ", Result: " << hr;
        
        // Attempt to hook IUnknown::AddRef for the created object
        IUnknown* pUnk = NULL;
        // Query for IUnknown interface
        HRESULT hrQuery = pUnkInstance->QueryInterface(IID_IUnknown, (void**)&pUnk);
        if (SUCCEEDED(hrQuery) && pUnk != NULL)
        {
            LOG(INFO) << "Successfully queried IUnknown interface.";
            // Get the vtable of the IUnknown interface
            PVOID* vtable = *(PVOID**)pUnk;
            // The AddRef method is typically the second entry in the IUnknown vtable (QueryInterface, AddRef, Release)
            PVOID originalAddRef = vtable[1];
            
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)True_AddRef, Mine_AddRef);
            LONG detour_result = DetourTransactionCommit();

            if (detour_result == NO_ERROR)
            {
                LOG(INFO) << "Successfully hooked IUnknown::AddRef.";
            }
            else
            {
                LOG(ERROR) << "Failed to hook IUnknown::AddRef. DetourTransactionCommit failed with error: " << detour_result;
            }
            
            // Release the queried IUnknown interface
            pUnk->Release();
        }
        else
        {
            LOG(ERROR) << "Failed to query IUnknown interface. HRESULT: " << hrQuery;
        }
    }
    else
    {
        // ... existing code ...
    }
} 