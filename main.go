package main

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"syscall"
	"time"

	"github.com/seekehr/DevSpoofGO/cli"
	"github.com/seekehr/DevSpoofGO/info"
	"github.com/seekehr/DevSpoofGO/logger"
)

func main() {
	admin := isAdmin()
	if !admin {
		logger.Error("Program is not running with administrator privileges", nil)
		os.Exit(1)
	}
	err := setWorkingDirToExecutable()
	if err != nil {
		logger.Error("Failed to set working directory to executable", err)
		os.Exit(1)
	}

	fmt.Println("Starting program...")
	startTime := time.Now()
	newInfo := info.NewInfo()
	fmt.Println("Info loaded in: " + time.Since(startTime).String())

	dll, err := loadDll()
	if err != nil {
		logger.Error("Failed to load DLL", err)
		os.Exit(1)
		return
	} else {
		logger.Success("DLL loaded successfully")
	}

	cli.Start(dll, newInfo, true) // removing later ;p
}

func isAdmin() bool {
	shell32 := syscall.NewLazyDLL("shell32.dll")
	isUserAnAdmin := shell32.NewProc("IsUserAnAdmin")

	ret, _, _ := isUserAnAdmin.Call()
	return ret != 0
}

func setWorkingDirToExecutable() error {
	_, filename, _, ok := runtime.Caller(0)
	if !ok {
		return errors.New("could not get caller info")
	}
	dir := filepath.Dir(filename)
	return os.Chdir(dir)
}

func loadDll() (*syscall.DLL, error) {
	dllPath, _ := filepath.Abs("dll/spoof_dll.dll")
	wd, _ := os.Getwd()
	if _, err := os.Stat(dllPath); errors.Is(err, os.ErrNotExist) {
		return nil, fmt.Errorf("DLL file does not exist! Working dir: %s, full path: %s", wd, dllPath)
	}
	fmt.Printf("Opening DLL from: %s\n", wd)
	dll, err := syscall.LoadDLL(dllPath)
	if err != nil {
		return nil, fmt.Errorf("failed to load DLL locally (%s): %w", dllPath, err)
	}
	return dll, nil
}
