package config

import (
	"errors"
	"fmt"
	generator "github.com/seekehr/DevSpoofGO/config/generator"
	"github.com/seekehr/DevSpoofGO/logger"
	"syscall"
	"unsafe"
)

var (
	kernel32                = syscall.NewLazyDLL("kernel32.dll")
	procVirtualAllocEx      = kernel32.NewProc("VirtualAllocEx")
	procVirtualFreeEx       = kernel32.NewProc("VirtualFreeEx")
	procWriteProcessMemory  = kernel32.NewProc("WriteProcessMemory")
	procCloseHandle         = kernel32.NewProc("CloseHandle")
	procCreateRemoteThread  = kernel32.NewProc("CreateRemoteThread")
	procWaitForSingleObject = kernel32.NewProc("WaitForSingleObject")
)

type ConfigValue string

const memCommit = 0x1000
const memReserve = 0x2000
const memRelease = 0x8000
const pageReadwrite = 0x04

const SetSpoofedPcName = ConfigValue("SetSpoofedComputerName")
const SetSpoofedVolumeSerial = ConfigValue("SetSpoofedVolumeSerial")
const SetSpoofedMotherboardSerial = ConfigValue("SetSpoofedMotherboardSerial")
const SetSpoofedMacAddress = ConfigValue("SetSpoofedMacAddress")
const SetSpoofedWlanGUID = ConfigValue("SetSpoofedWlanGUID")
const SetSpoofedBSSID = ConfigValue("SetSpoofedBSSID")
const SetSpoofedBiosSerial = ConfigValue("SetSpoofedBiosSerial")
const SetSpoofedProcessorId = ConfigValue("SetSpoofedProcessorId")
const SetSpoofedSystemUuid = ConfigValue("SetSpoofedSystemUuid")
const SetSpoofedDiskSerial = ConfigValue("SetSpoofedDiskSerial")
const SetSpoofedMachineGuid = ConfigValue("SetSpoofedMachineGuid")

type Config struct {
	localDll      *syscall.DLL
	dllHandle     syscall.Handle
	processHandle syscall.Handle
	values        generator.RandomGeneratedValues
	Closed        bool
}

func New(dll *syscall.DLL, handle syscall.Handle, processHandle syscall.Handle) Config {
	return Config{
		localDll:      dll,
		dllHandle:     handle,
		processHandle: processHandle,
		values:        generator.GenerateRandomValues(),
		Closed:        false,
	}
}

// ConfigureDLL initial function to set default values
func (c *Config) ConfigureDLL() error {
	// Set the values in the DLL
	if err := c.SetValue(SetSpoofedPcName, c.values.Hostname); err != nil {
		return fmt.Errorf("failed to set spoofed computer name: %w", err)
	}
	if err := c.SetValue(SetSpoofedVolumeSerial, c.values.VolumeSerial); err != nil {
		return fmt.Errorf("failed to set spoofed volume serial: %w", err)
	}
	if err := c.SetValue(SetSpoofedMotherboardSerial, c.values.Motherboard); err != nil {
		return fmt.Errorf("failed to set spoofed motherboard serial: %w", err)
	}

	if err := c.SetValue(SetSpoofedProcessorId, c.values.ProcessorID); err != nil {
		return fmt.Errorf("failed to set spoofed processor id: %w", err)
	}

	if err := c.SetValue(SetSpoofedBiosSerial, c.values.BIOS); err != nil {
		return fmt.Errorf("failed to set spoofed bios serial: %w", err)
	}

	if err := c.SetValue(SetSpoofedMacAddress, c.values.MAC); err != nil {
		return fmt.Errorf("failed to set spoofed MAC address: %w", err)
	}

	if err := c.SetValue(SetSpoofedWlanGUID, c.values.WlanGUID); err != nil {
		return fmt.Errorf("failed to set spoofed WLAN GUID: %w", err)
	}

	if err := c.SetValue(SetSpoofedBSSID, c.values.BSSID); err != nil {
		return fmt.Errorf("failed to set spoofed BSSID: %w", err)
	}

	if err := c.SetValue(SetSpoofedSystemUuid, c.values.UUID); err != nil {
		return fmt.Errorf("failed to set spoofed system UUID: %w", err)
	}

	if err := c.SetValue(SetSpoofedDiskSerial, c.values.DiskSerial); err != nil {
		return fmt.Errorf("failed to set spoofed disk serial: %w", err)
	}

	if err := c.SetValue(SetSpoofedMachineGuid, c.values.MachineGUID); err != nil {
		return fmt.Errorf("failed to set spoofed machine GUID: %w", err)
	}
	
	return nil
}

func (c *Config) calculateOffset(functionName ConfigValue) (uintptr, error) {
	// get the address in our own memory
	proc, err := syscall.GetProcAddress(c.localDll.Handle, string(functionName))
	if proc == 0 {
		if err != nil {
			return 0, fmt.Errorf("failed to get address of function %s in local DLL. %w", functionName, err)
		}
		return 0, fmt.Errorf("failed to get address of function %s in local DLL (address is 0)", functionName)
	}

	// calculate offset (address in our own memory - address in the target process)
	offset := proc - uintptr(c.dllHandle)
	return offset, nil
}

func (c *Config) SetValue(name ConfigValue, value generator.RandomGeneratedValue) error {
	if c.Closed {
		return errors.New("config is closed")
	}

	// get offset to calculate func address
	funcOffset, err := c.calculateOffset(name)
	if err != nil {
		return fmt.Errorf("failed to calculate offset for function %s: %w", name, err)
	}
	funcAddr := uintptr(c.dllHandle) + funcOffset

	valueBytes := append([]byte(value), 0) // null-terminated C string

	// allocate memory to store our generated value to write to it later
	sizeToAllocate := uintptr(len(valueBytes))
	remoteStringMem, _, err := syscall.SyscallN(
		procVirtualAllocEx.Addr(),     // Address of VirtualAllocEx in our process
		uintptr(c.processHandle),      // Handle to the target process (received in the injector)
		uintptr(0),                    // Desired starting address (0 lets the OS decide)
		sizeToAllocate,                // Size of the memory region to allocate
		uintptr(memCommit|memReserve), // Allocation type (reserve and commit)
		uintptr(pageReadwrite),        // Memory protection (can read and write)
	)

	if remoteStringMem == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("exception encountered @ allocating memory with valid error: %w", err)
		}
		return fmt.Errorf("failed to allocate remote virtual address. Size to allocate: %d", sizeToAllocate)
	}

	// clean-up allocated memory
	defer func() {
		// VirtualFreeEx returns non-zero on success
		ret, _, err := syscall.SyscallN(
			procVirtualFreeEx.Addr(), // Address of VirtualFreeEx in our process
			uintptr(c.processHandle), // Handle to the target process (received in injector.go ;3)
			remoteStringMem,          // Base address of the memory to free
			uintptr(0),               // Size (0 means the entire region reserved in VirtualAllocEx)
			uintptr(memRelease),      // Free type (release the region)
		)

		if ret == 0 {
			if !errors.Is(err, syscall.Errno(0)) {
				logger.Error(fmt.Sprintf("Warning: Failed to VirtualFreeEx remote memory at 0x%X.", remoteStringMem), err)
			} else {
				logger.Error(fmt.Sprintf("Warning: Failed to VirtualFreeEx remote memory at 0x%X (return code is 0).", remoteStringMem), nil)
			}
		}
	}()

	// write the value to the allocated memory in the target process
	writtenBytes := uintptr(0)
	code, _, err := syscall.SyscallN(
		procWriteProcessMemory.Addr(),
		uintptr(c.processHandle),
		remoteStringMem,
		uintptr(unsafe.Pointer(&valueBytes[0])),
		sizeToAllocate,
		uintptr(unsafe.Pointer(&writtenBytes)), // receive how many bytes written
	)

	if code == 0 || writtenBytes != sizeToAllocate {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("exception encountered @ writing memory with valid error: %w", err)
		}

		return fmt.Errorf("failed to WriteProcessMemory (code is 0 or written bytes mismatch). Got code: %d. Expected %d, wrote %d", code, sizeToAllocate, writtenBytes)
	}

	// remote thread to execute the code in ANOTHER PROCESS
	// CreateRemoteThread(hProcess, lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId)
	// This is the correct way to initiate code execution in another process.
	threadTargetFunc, _, err := syscall.SyscallN(
		procCreateRemoteThread.Addr(),
		uintptr(c.processHandle), // Handle to the target process (tells the kernel *which* process)
		uintptr(0),               // Default security attributes
		uintptr(0),               // Default stack size
		funcAddr,                 // Start address is the target function address *in the remote process* (this address is only valid there)
		remoteStringMem,          // Parameter is the address of the value string *in the remote process* (this address is also only valid there)
		uintptr(0),               // Creation flags (0 for run immediately)
		uintptr(0),               // Pointer to thread ID (can be 0 if not needed)
	)

	if threadTargetFunc == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("failed to CreateRemoteThread for '%s' call: %w", name, err)
		}
		return fmt.Errorf("failed to CreateRemoteThread for '%s' call (handle is 0)", name)
	}
	defer procCloseHandle.Call(threadTargetFunc)

	// wait for the remote thread to be complete
	waitResult, _, err := syscall.SyscallN(
		procWaitForSingleObject.Addr(),
		threadTargetFunc,
		uintptr(5000), // 5 seconds
	)

	// Check if WaitForSingleObject itself failed or timed out
	if waitResult != syscall.WAIT_OBJECT_0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("WaitForSingleObject for '%s' call failed or timed out: %w", name, err)
		}

		return fmt.Errorf("WaitForSingleObject for '%s' call returned timeout result: %d", name, waitResult)
	}

	return nil
}

func (c *Config) Close() {
	// CLOSE HANDLE TO DLL INSIDE THE PROCESS
	ret, _, closeErr := procCloseHandle.Call(uintptr(c.dllHandle))
	if ret == 0 {
		if closeErr != nil && closeErr.(syscall.Errno) != 0 {
			logger.Error(fmt.Sprintf("Warning: Failed to close dll handle 0x%X.", c.dllHandle), closeErr)
		} else {
			logger.Error(fmt.Sprintf("Warning: Failed to close dll handle 0x%X (ret is 0).", c.dllHandle), nil)
		}
	} else {
		logger.Success(fmt.Sprintf("Successfully closed dll handle 0x%X.\n", c.dllHandle))
	}

	// CLOSE HANDLE TO PROCESS
	ret, _, closeErr = procCloseHandle.Call(uintptr(c.processHandle))
	if ret == 0 {
		if closeErr != nil && closeErr.(syscall.Errno) != 0 {
			logger.Error(fmt.Sprintf("Warning: Failed to close process handle 0x%X.", c.processHandle), closeErr)
		} else {
			logger.Error(fmt.Sprintf("Warning: Failed to close process handle 0x%X (ret is 0).", c.processHandle), nil)
		}
	} else {
		logger.Success(fmt.Sprintf("Successfully closed process handle 0x%X.\n", c.processHandle))
	}

	// CLOSE LOCAL DLL
	c.localDll.Release()
	c.Closed = true
}
