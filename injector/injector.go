package injector

import (
	"syscall"
	"unsafe"
)

var (
	kernel32                = syscall.NewLazyDLL("kernel32.dll")
	procOpenProcess         = kernel32.NewProc("OpenProcess")
	procVirtualAllocEx      = kernel32.NewProc("VirtualAllocEx")
	procWriteProcessMemory  = kernel32.NewProc("WriteProcessMemory")
	procGetProcAddress      = kernel32.NewProc("GetProcAddress")
	procLoadLibraryA        = kernel32.NewProc("LoadLibraryA")
	procCreateRemoteThread  = kernel32.NewProc("CreateRemoteThread")
	procWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")
	procCloseHandle         = kernel32.NewProc("CloseHandle")
)

const (
	PROCESS_ALL_ACCESS = 0x1F0FFF
	MEM_COMMIT         = 0x1000
	MEM_RESERVE        = 0x2000
	PAGE_READWRITE     = 0x04
)

func Inject(pid int, dllPath string) error {
	// Open the target process
	handle, _, err := procOpenProcess.Call(
		uintptr(PROCESS_ALL_ACCESS),
		uintptr(0),
		uintptr(pid),
	)
	if handle == 0 {
		return err
	}
	defer procCloseHandle.Call(handle)

	// Allocate memory in the target process
	dllPathBytes := []byte(dllPath)
	remoteMem, _, err := procVirtualAllocEx.Call(
		handle,
		uintptr(0),
		uintptr(len(dllPathBytes)+1),
		uintptr(MEM_COMMIT|MEM_RESERVE),
		uintptr(PAGE_READWRITE),
	)
	if remoteMem == 0 {
		return err
	}

	// Write the DLL path to the allocated memory
	_, _, err = procWriteProcessMemory.Call(
		handle,
		remoteMem,
		uintptr(unsafe.Pointer(&dllPathBytes[0])),
		uintptr(len(dllPathBytes)),
		uintptr(0),
	)
	if err != nil {
		return err
	}

	// Get the address of LoadLibraryA
	loadLibraryAddr, _, err := procGetProcAddress.Call(
		procLoadLibraryA.Addr(),
		uintptr(0),
	)
	if loadLibraryAddr == 0 {
		return err
	}

	// Create a remote thread to load the DLL
	thread, _, err := procCreateRemoteThread.Call(
		handle,
		uintptr(0),
		uintptr(0),
		loadLibraryAddr,
		remoteMem,
		uintptr(0),
		uintptr(0),
	)
	if thread == 0 {
		return err
	}
	defer procCloseHandle.Call(thread)

	// Wait for the thread to complete
	_, _, err = procWaitForSingleObject.Call(
		thread,
		uintptr(0xFFFFFFFF),
	)
	if err != nil {
		return err
	}

	return nil
}
