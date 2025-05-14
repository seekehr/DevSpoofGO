package config

import (
	"errors"
	"path/filepath"
	"strconv"
	"syscall"
	"unsafe"

	generator "github.com/seekehr/DevSpoofGO/config/generator"
)

// DLLHandler provides an interface to configure the spoofing DLL
type DLLHandler struct {
	dll             *syscall.LazyDLL
	setComputerName *syscall.LazyProc
	setVolumeSerial *syscall.LazyProc
	setProcessorId  *syscall.LazyProc
}

// NewDLLHandler creates a new handler for the spoofing DLL
func NewDLLHandler(dllPath string) (*DLLHandler, error) {
	// If no path provided, use the default path relative to executable
	if dllPath == "" {
		dllPath = filepath.Join("dll", "spoof_dll.dll")
	}

	dll := syscall.NewLazyDLL(dllPath)
	err := dll.Load()
	if err != nil {
		return nil, err
	}

	handler := &DLLHandler{
		dll:             dll,
		setComputerName: dll.NewProc("SetSpoofedComputerName"),
		setVolumeSerial: dll.NewProc("SetSpoofedVolumeSerial"),
		setProcessorId:  dll.NewProc("SetSpoofedProcessorId"),
	}

	return handler, nil
}

// stringToCharPtr converts a Go string to a C-style null-terminated string
func stringToCharPtr(s string) uintptr {
	// Add null terminator
	bytes := append([]byte(s), 0)
	return uintptr(unsafe.Pointer(&bytes[0]))
}

// ConfigureDLL configures the spoofing DLL with the randomly generated values
func (h *DLLHandler) ConfigureDLL(values generator.RandomGeneratedValues) error {
	// Set computer name (hostname)
	_, _, err := h.setComputerName.Call(stringToCharPtr(values.Hostname))
	if err != nil && !errors.Is(err, syscall.Errno(0)) { // syscall.Errno(0) means success
		return err
	}

	// Set processor ID
	_, _, err = h.setProcessorId.Call(stringToCharPtr(values.ProcessorID))
	if err != nil && !errors.Is(err, syscall.Errno(0)) {
		return err
	}

	// Convert volume serial number from string to DWORD
	volumeSerial, err := strconv.ParseUint(values.VolumeSerial, 10, 32)
	if err != nil {
		return err
	}

	// Set volume serial number
	_, _, err = h.setVolumeSerial.Call(uintptr(volumeSerial))
	if err != nil && !errors.Is(err, syscall.Errno(0)) {
		return err
	}

	return nil
}

// ConfigureAllFromRandom creates a DLL handler and configures it with newly generated random values
// Returns the generated values and any error that occurred
func ConfigureAllFromRandom(dllPath string) (generator.RandomGeneratedValues, error) {
	// Generate random values
	values := generator.GenerateRandomValues()

	// Create DLL handler
	handler, err := NewDLLHandler(dllPath)
	if err != nil {
		return values, err
	}

	// Configure DLL with random values
	err = handler.ConfigureDLL(values)
	return values, err
}

// ConfigureAllWithValues creates a DLL handler and configures it with the provided values
func ConfigureAllWithValues(dllPath string, values generator.RandomGeneratedValues) error {
	// Create DLL handler
	handler, err := NewDLLHandler(dllPath)
	if err != nil {
		return err
	}

	// Configure DLL with provided values
	return handler.ConfigureDLL(values)
}
