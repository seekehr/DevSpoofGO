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
	procGetProcAddress      = kernel32.NewProc("GetProcAddress") // Not strictly needed for basic LoadLibraryA injection but good to have
	procLoadLibraryA        = kernel32.NewProc("LoadLibraryA")
	procCreateRemoteThread  = kernel32.NewProc("CreateRemoteThread")
	procWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")
	procCloseHandle         = kernel32.NewProc("CloseHandle")
	procGetExitCodeThread   = kernel32.NewProc("GetExitCodeThread") // Add this
)

const (
	PROCESS_ALL_ACCESS = 0x1F0FFF
	MEM_COMMIT         = 0x1000
	MEM_RESERVE        = 0x2000
	PAGE_READWRITE     = 0x04
	INFINITE           = 0xFFFFFFFF // Define INFINITE for WaitForSingleObject
)

func Inject(pid int) error {
	// Open the target process
	dllPath, _ := filepath.Abs("dll/spoof_dll.dll") // Path to the DLL to inject
	if _, err := os.Stat(dllPath); errors.Is(err, os.ErrNotExist) {
		wd, _ := os.Getwd()
		return fmt.Errorf("DLL file does not exist! Working directory: %s", wd)
	}

	handle, _, err := procOpenProcess.Call(
		uintptr(PROCESS_ALL_ACCESS),
		uintptr(0),
		uintptr(pid),
	)
	if handle == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("failed to OpenProcess: %w", err)
		}
		return errors.New("failed to OpenProcess (handle is 0 and err is nil)")
	}
	defer procCloseHandle.Call(handle)

	// Allocate memory in the target process
	dllPathBytes := append([]byte(dllPath), 0) // Null-terminate the string
	remoteMem, _, err := procVirtualAllocEx.Call(
		handle,
		uintptr(0),
		uintptr(len(dllPathBytes)), // Allocate size including the null terminator
		uintptr(MEM_COMMIT|MEM_RESERVE),
		uintptr(PAGE_READWRITE),
	)
	if remoteMem == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("failed to VirtualAllocEx: %w", err)
		}
		return errors.New("failed to VirtualAllocEx (address is 0 and err is nil)")
	}
	fmt.Println("hi")

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
			return fmt.Errorf("failed to WriteProcessMemory: %w", err)
		}
		// If err is nil, it means the Call succeeded but wrote 0 bytes or the wrong amount
		return fmt.Errorf("failed to WriteProcessMemory (code is 0 or written bytes mismatch). Expected %d, wrote %d", len(dllPathBytes), writtenBytes)
	}
	fmt.Println("hi2")

	// Get the address of LoadLibraryA
	loadLibraryAddr := procLoadLibraryA.Addr()
	if loadLibraryAddr == 0 {
		// LoadLibraryA.Addr() should not typically fail unless kernel32.dll isn't found,
		// which is highly unlikely. The err from Addr() is usually nil.
		return errors.New("failed to get address of LoadLibraryA")
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
			return fmt.Errorf("failed to CreateRemoteThread: %w", err)
		}
		return errors.New("failed to CreateRemoteThread (handle is 0 and err is nil)")
	}
	fmt.Println("hi3")
	defer procCloseHandle.Call(thread)

	// Wait for the thread to complete
	// WaitForSingleObject returns WAIT_OBJECT_0 (0) on success
	waitResult, _, err := procWaitForSingleObject.Call(
		thread,
		uintptr(INFINITE),
	)

	// Check if WaitForSingleObject itself failed
	if waitResult != syscall.WAIT_OBJECT_0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("WaitForSingleObject failed or timed out: %w", err)
		}
		// This case is less likely with INFINITE timeout, but handle it
		return fmt.Errorf("WaitForSingleObject returned unexpected result: %d", waitResult)
	}

	fmt.Println("hi4") // This should now be printed if WaitForSingleObject succeeds

	// Get the exit code of the remote thread (the return value of LoadLibraryA)
	var exitCode uint32
	// GetExitCodeThread returns non-zero on success
	// syscall.Syscall is used here because GetExitCodeThread takes a pointer to a DWORD
	ret, _, err := syscall.Syscall(procGetExitCodeThread.Addr(), 2,
		thread,
		uintptr(unsafe.Pointer(&exitCode)),
		0)

	if ret == 0 { // GetExitCodeThread failed
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("failed to GetExitCodeThread after waiting: %w", err)
		}
		return errors.New("failed to GetExitCodeThread after waiting (ret is 0 and err is nil)")
	}

	// Check the exit code (LoadLibraryA's return value)
	if exitCode == 0 {
		// LoadLibraryA returned NULL, indicating failure to load the DLL
		return errors.New("loadLibraryA failed in the remote process (returned NULL). Possible reasons: DLL not found, missing dependencies, architecture mismatch, or security restrictions")
	}

	// If we reached here, LoadLibraryA returned a non-NULL address (exitCode)
	// which is the base address of the loaded DLL. Injection was successful.
	fmt.Printf("DLL injected successfully. Base address: 0x%X\n", exitCode)
	return nil
}
