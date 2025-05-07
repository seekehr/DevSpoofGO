package info_os

import (
	"os"
	"os/exec"
	"strings"
	"sync"

	logger "github.com/seekehr/DevSpoofGO/logger"
)

type OSInfo struct {
	hostname     string
	installDate  string
	serialNumber string
	userSID      string

	lock sync.Mutex
}

func NewOSInfo() *OSInfo {
	return &OSInfo{
		hostname:     getHostname(),
		installDate:  getWindowsInstallDate(),
		serialNumber: getOSSerialNumber(),
		userSID:      getUserSID(),
		lock:         sync.Mutex{},
	}
}

func (o *OSInfo) GetHostname() string {
	return o.hostname
}

func (o *OSInfo) GetInstallDate() string {
	return o.installDate
}

func (o *OSInfo) GetSerialNumber() string {
	return o.serialNumber
}

func (o *OSInfo) GetUserSID() string {
	return o.userSID
}

func getWindowsInstallDate() string {
	cmd := exec.Command("wmic", "os", "get", "installdate")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get Windows install date", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse Windows install date", nil)
	return ""
}

func getOSSerialNumber() string {
	cmd := exec.Command("wmic", "os", "get", "serialnumber")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get OS serial number", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse OS serial number", nil)
	return ""
}

func getUserSID() string {
	// Get current username first
	usernameCmd := exec.Command("whoami")
	usernameOutput, err := usernameCmd.Output()
	if err != nil {
		logger.Error("Failed to get current username", err)
		return ""
	}
	username := strings.TrimSpace(string(usernameOutput))

	// Now get the SID for this username
	sidCmd := exec.Command("wmic", "useraccount", "where", "name='"+strings.Split(username, "\\")[1]+"'", "get", "sid")
	sidOutput, err := sidCmd.Output()
	if err != nil {
		logger.Error("Failed to get user SID", err)
		return ""
	}

	lines := strings.Split(string(sidOutput), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse user SID", nil)
	return ""
}

func getHostname() string {
	hostname, err := os.Hostname()
	if err != nil {
		logger.Error("Failed to get hostname", err)
		return ""
	}
	return hostname
}
