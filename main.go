package main

import (
	"fmt"
	"github.com/seekehr/DevSpoofGO/cli"
	"github.com/seekehr/DevSpoofGO/info"
	"github.com/seekehr/DevSpoofGO/logger"
	"os"
	"syscall"
	"time"
)

func main() {
	admin := isAdmin()
	logger.Init()
	if !admin {
		logger.Error("Program is not running with administrator privileges", nil)
		os.Exit(1)
	}

	fmt.Println("Starting program...")
	startTime := time.Now()
	newInfo := info.NewInfo()
	fmt.Println("Info loaded in: " + time.Since(startTime).String())
	cli.Start(newInfo)
}

func isAdmin() bool {
	shell32 := syscall.NewLazyDLL("shell32.dll")
	isUserAnAdmin := shell32.NewProc("IsUserAnAdmin")

	ret, _, _ := isUserAnAdmin.Call()
	return ret != 0
}
