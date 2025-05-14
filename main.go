package main

import (
	"fmt"
	generator "github.com/seekehr/DevSpoofGO/config/generator"
	"os"
	"syscall"
	"time"
	"unsafe"

	cli "github.com/seekehr/DevSpoofGO/cli"
	config "github.com/seekehr/DevSpoofGO/config"
	info "github.com/seekehr/DevSpoofGO/info"
	logger "github.com/seekehr/DevSpoofGO/logger"
)

func main() {
	admin := isAdmin()
	if !admin {
		logger.Error("Program is not running with administrator privileges", nil)
		os.Exit(1)
	}

	logger.Info("Starting program...")
	startTime := time.Now()
	newInfo := info.NewInfo()
	logger.Info("Info loaded in: " + time.Since(startTime).String())
	handler, err := config.NewDLLHandler("")
	if handler == nil || err != nil {
		logger.Error("Failed to load DLL handler", err)
		os.Exit(1)
	} else {
		logger.Success("DLL handler loaded successfully")
		err := handler.ConfigureDLL(generator.GenerateRandomValues())
		if err != nil {
			logger.Error("Failed to configure DLL", err)
			return
		}
		logger.Success("DLL configured successfully")
		kernel32 := syscall.MustLoadDLL("kernel32.dll")
		getComputerName := kernel32.MustFindProc("GetComputerNameA")

		var buffer [256]byte
		size := uint32(len(buffer))

		r1, _, err := getComputerName.Call(uintptr(unsafe.Pointer(&buffer[0])), uintptr(unsafe.Pointer(&size)))
		if r1 == 0 {
			fmt.Println("GetComputerName failed:", err)
			return
		}
		fmt.Println("Spoofed Computer Name:", string(buffer[:size]))
	}
	cli.Start(newInfo)
}

func isAdmin() bool {
	shell32 := syscall.NewLazyDLL("shell32.dll")
	isUserAnAdmin := shell32.NewProc("IsUserAnAdmin")

	ret, _, _ := isUserAnAdmin.Call()
	return ret != 0
}
