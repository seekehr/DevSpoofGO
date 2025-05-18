package debug

import (
	"syscall"
	"unsafe"
)

func MotherboardSerial(dll *syscall.DLL) {
	// Get the address of the function in the DLL
	proc, err := dll.FindProc("GetOriginalMotherboardSerial")
	if err != nil {
		panic(err)
	}

	// Call the function
	ret, _, _ := proc.Call()

	// Convert the return value (which is a C string pointer) to a Go string
	// This reads the null-terminated C string at the memory address returned by the function
	if ret != 0 {
		cstring := (*byte)(unsafe.Pointer(ret))
		var builder []byte
		for *cstring != 0 {
			builder = append(builder, *cstring)
			cstring = (*byte)(unsafe.Pointer(uintptr(unsafe.Pointer(cstring)) + 1))
		}

		// Print the actual string content
		println("Motherboard Serial:", string(builder))
	} else {
		println("Motherboard Serial: NULL pointer returned")
	}
}
