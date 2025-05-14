package main

import (
	cli "github.com/seekehr/DevSpoofGO/cli"
	info "github.com/seekehr/DevSpoofGO/info"
	logger "github.com/seekehr/DevSpoofGO/logger"
	"os"
	"syscall"
	"time"
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
	cli.Start(newInfo)
}

func isAdmin() bool {
	shell32 := syscall.NewLazyDLL("shell32.dll")
	isUserAnAdmin := shell32.NewProc("IsUserAnAdmin")

	ret, _, _ := isUserAnAdmin.Call()
	return ret != 0
}
