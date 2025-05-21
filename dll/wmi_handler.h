// Type definition for IUnknown::AddRef
typedef ULONG(STDMETHODCALLTYPE* pfnAddRef)(IUnknown* This);

// Trampoline for the original IUnknown::AddRef
extern pfnAddRef True_AddRef;

// Detour function for IUnknown::AddRef
ULONG STDMETHODCALLTYPE Mine_AddRef(IUnknown* This); 