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
	kernel32               = syscall.NewLazyDLL("kernel32.dll")
	procVirtualAllocEx     = kernel32.NewProc("VirtualAllocEx")
	procVirtualFreeEx      = kernel32.NewProc("VirtualFreeEx")
	procWriteProcessMemory = kernel32.NewProc("WriteProcessMemory")
	procCloseHandle        = kernel32.NewProc("CloseHandle")
)

type ConfigValue string

const memCommit = 0x1000
const memReserve = 0x2000
const memRelease = 0x8000
const pageReadwrite = 0x04

const SetSpoofedPcName = ConfigValue("SetSpoofedComputerName")
const SetSpoofedVolSerial = ConfigValue("SetSpoofedVolumeSerial")
const SetSpoofedProcId = ConfigValue("SetSpoofedProcessorId")

type Config struct {
	dllHandle     syscall.Handle
	processHandle syscall.Handle
	values        generator.RandomGeneratedValues
	Closed        bool
}

func New(handle syscall.Handle, processHandle syscall.Handle) Config {
	return Config{
		dllHandle:     handle,
		processHandle: processHandle,
		values:        generator.GenerateRandomValues(),
		Closed:        false,
	}
}

func (c *Config) ConfigureDLL() error {

	// Set the values in the DLL
	if err := c.SetValue(SetSpoofedPcName, c.values.Hostname); err != nil {
		return fmt.Errorf("failed to set spoofed computer name: %w", err)
	}
	if err := c.SetValue(SetSpoofedVolSerial, c.values.VolumeSerial); err != nil {
		return fmt.Errorf("failed to set spoofed volume serial: %w", err)
	}
	if err := c.SetValue(SetSpoofedProcId, c.values.ProcessorID); err != nil {
		return fmt.Errorf("failed to set spoofed processor ID: %w", err)
	}

	return nil
}

func (c *Config) SetValue(name ConfigValue, value generator.RandomGeneratedValue) error {
	if c.Closed {
		return errors.New("config is closed")
	}

	fmt.Printf("Dllhandle: 0x%X\nProcessHandle: 0x%X\n", c.dllHandle, c.processHandle)
	// get address of function from the instance of dll module inside the process
	proc, err := syscall.GetProcAddress(c.dllHandle, string(name))
	if err != nil {
		return fmt.Errorf("exception encountered @ getting process func addr with valid error: %w", err)
	}

	// allocate memory to store our generated value to write to it later
	sizeToAllocate := uintptr(len(value))
	remoteStringMem, _, err := syscall.SyscallN(
		procVirtualAllocEx.Addr(),     // Address of VirtualAllocEx in our process
		uintptr(c.processHandle),      // <-- Handle to the target process
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
			uintptr(c.processHandle), // <-- Handle to the target process
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
		} else {
			logger.Success(fmt.Sprintf("Cleaned up remote memory at 0x%X\n", remoteStringMem))
		}
	}()

	// write the value to the allocated memory in the target process
	writtenBytes := uintptr(0)
	code, _, err := syscall.SyscallN(
		procWriteProcessMemory.Addr(),
		uintptr(c.processHandle),
		remoteStringMem,
		uintptr(unsafe.Pointer(&value)),
		sizeToAllocate,
		uintptr(unsafe.Pointer(&writtenBytes)), // Pass address to receive bytes written
	)

	if code == 0 || writtenBytes != sizeToAllocate {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("exception encountered @ writing memory with valid error: %w", err)
		}

		return fmt.Errorf("failed to WriteProcessMemory (code is 0 or written bytes mismatch). Got code: %d. Expected %d, wrote %d", code, sizeToAllocate, writtenBytes)
	}

	// call the function in the target process (and give it the mem addr we just wrote our targeted value to)
	ret, _, err := syscall.SyscallN(
		proc, // Address of the function to call IN THE TARGET PROCESS
		remoteStringMem,
	)
	if ret == 0 {
		if err != nil && err.(syscall.Errno) != 0 {
			return fmt.Errorf("exception encountered @ calling target function with valid error: %w", err)
		}

		return fmt.Errorf("failed to call function %s in target process (return code is 0)", string(name))
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

	c.Closed = true
}
