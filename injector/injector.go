package injector

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"syscall"
	"unsafe"
)

var (
	kernel32                = syscall.NewLazyDLL("kernel32.dll")
	procOpenProcess         = kernel32.NewProc("OpenProcess")
	procVirtualAllocEx      = kernel32.NewProc("VirtualAllocEx")
	procWriteProcessMemory  = kernel32.NewProc("WriteProcessMemory")
	procLoadLibraryA        = kernel32.NewProc("LoadLibraryA")
	procCreateRemoteThread  = kernel32.NewProc("CreateRemoteThread")
	procWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")
	procCloseHandle         = kernel32.NewProc("CloseHandle")
	procGetExitCodeThread   = kernel32.NewProc("GetExitCodeThread") // Add this
)

const (
	ProcessAllAccess = 0x1F0FFF
)

const (
	MemCommit     = 0x1000
	MemReserve    = 0x2000
	PageReadwrite = 0x04
	INFINITE      = 0xFFFFFFFF
)

func Inject(pid int) (uintptr, uintptr, error) {
	// Open the target process
	dllPath, _ := filepath.Abs("dll/spoof_dll.dll") // Path to the DLL to inject
	if _, err := os.Stat(dllPath); errors.Is(err, os.ErrNotExist) {
		wd, _ := os.Getwd()
		return 0, 0, fmt.Errorf("DLL file does not exist! Working directory: %s", wd)
	}

	handle, _, err := procOpenProcess.Call(
		uintptr(ProcessAllAccess),
		uintptr(0),
		uintptr(pid),
	)
	if handle == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return 0, 0, fmt.Errorf("failed to OpenProcess: %w", err)
		}
		return 0, 0, errors.New("failed to OpenProcess (handle is 0 and err is nil)")
	}

	// Allocate memory in the target process
	dllPathBytes := append([]byte(dllPath), 0) // Null-terminate the string
	remoteMem, _, err := procVirtualAllocEx.Call(
		handle,
		uintptr(0),
		uintptr(len(dllPathBytes)), // Allocate size including the null terminator
		uintptr(MemCommit|MemReserve),
		uintptr(PageReadwrite),
	)
	if remoteMem == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return 0, 0, fmt.Errorf("failed to VirtualAllocEx: %w", err)
		}
		return 0, 0, errors.New("failed to VirtualAllocEx (address is 0 and err is nil)")
	}

	// Write the DLL path to the allocated memory
	// WriteProcessMemory returns non-zero on success
	writtenBytes := uintptr(0) // Variable to receive the number of bytes written
	code, _, err := procWriteProcessMemory.Call(
		handle,
		remoteMem,
		uintptr(unsafe.Pointer(&dllPathBytes[0])),
		uintptr(len(dllPathBytes)),
		uintptr(unsafe.Pointer(&writtenBytes)), // Pass address to receive bytes written
	)
	// Check if the call itself failed (code is 0) OR if the number of written bytes
	// does not match the expected size.
	if code == 0 || writtenBytes != uintptr(len(dllPathBytes)) {
		if err != nil && err.(syscall.Errno) != 0 {
			return 0, 0, fmt.Errorf("failed to WriteProcessMemory: %w", err)
		}
		// If err is nil, it means the Call succeeded but wrote 0 bytes or the wrong amount
		return 0, 0, fmt.Errorf("failed to WriteProcessMemory (code is 0 or written bytes mismatch). Expected %d, wrote %d", len(dllPathBytes), writtenBytes)
	}

	// Get the address of LoadLibraryA
	loadLibraryAddr := procLoadLibraryA.Addr()
	if loadLibraryAddr == 0 {
		// LoadLibraryA.Addr() should not typically fail unless kernel32.dll isn't found,
		// which is highly unlikely. The err from Addr() is usually nil.
		return 0, 0, errors.New("failed to get address of LoadLibraryA")
	}

	// Create a remote thread to load the DLL
	thread, _, err := procCreateRemoteThread.Call(
		handle,
		uintptr(0),      // Default security attributes
		uintptr(0),      // Default stack size
		loadLibraryAddr, // Start address of the thread (LoadLibraryA)
		remoteMem,       // Argument to the thread function (DLL path address)
		uintptr(0),      // Creation flags (0 for run immediately)
		uintptr(0),      // Pointer to thread ID (can be 0 if not needed)
	)
	if thread == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return 0, 0, fmt.Errorf("failed to CreateRemoteThread: %w", err)
		}
		return 0, 0, errors.New("failed to CreateRemoteThread (handle is 0 and err is nil)")
	}

	// Wait for the thread to complete
	// WaitForSingleObject returns WAIT_OBJECT_0 (0) on success
	waitResult, _, err := procWaitForSingleObject.Call(
		thread,
		uintptr(INFINITE),
	)

	// Check if WaitForSingleObject itself failed
	if waitResult != syscall.WAIT_OBJECT_0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return 0, 0, fmt.Errorf("WaitForSingleObject failed or timed out: %w", err)
		}
		// This case is less likely with INFINITE timeout, but handle it
		return 0, 0, fmt.Errorf("WaitForSingleObject returned unexpected result: %d", waitResult)
	}

	// GetExitCodeThread returns non-zero on success
	var exitCode uint32
	ret, _, err := procGetExitCodeThread.Call(
		thread,
		uintptr(unsafe.Pointer(&exitCode)),
	)

	if ret == 0 { // GetExitCodeThread failed
		if err != nil && err.(syscall.Errno) != 0 {
			return 0, 0, fmt.Errorf("failed to GetExitCodeThread after waiting: %w", err)
		}
		return 0, 0, errors.New("failed to GetExitCodeThread after waiting (ret is 0 and err is nil)")
	}

	// Check the exit code (LoadLibraryA's return value)
	if exitCode == 0 {
		// LoadLibraryA returned NULL, indicating failure to load the DLL
		return 0, 0, errors.New("loadLibraryA failed in the remote process (returned NULL). Possible reasons: DLL not found, missing dependencies, architecture mismatch, or security restrictions")
	}

	// LoadLibraryA succeeded and returned a non-NULL HMODULE/HINSTANCE
	// exitCode holds this handle value (as a uint32).
	// Cast it to uintptr for correct representation as a handle.
	injectedDLLModule := uintptr(exitCode)

	return injectedDLLModule, handle, nil // Return the handle to the process' instance of the DLL module AND the handle to the process
}
