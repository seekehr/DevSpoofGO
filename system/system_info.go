package system

import (
	"net"
	"os"
	"sync"

	logger "github.com/seekehr/DevSpoofGO/logger"
)

type SystemInfo struct {
	hostname string
	mac      string
	lock     sync.Mutex
}

func NewSystemInfo() *SystemInfo {
	return &SystemInfo{
		hostname: getHostname(),
		mac:      getMacAddress(),
		lock:     sync.Mutex{},
	}
}

func (s *SystemInfo) GetSystemInfo() *SystemInfo {
	return s
}

func (s *SystemInfo) GetHostname() string {
	return s.hostname
}

func (s *SystemInfo) GetMacAddress() string {
	return s.mac
}

func getHostname() string {
	hostname, err := os.Hostname()
	if err != nil {
		logger.Error("Failed to get hostname", err)
		return ""
	}
	return hostname
}

func getMacAddress() string {
	interfaces, err := net.Interfaces()
	if err != nil {
		logger.Error("Failed to get network interfaces", err)
		return ""
	}
	for _, iface := range interfaces {
		if iface.Flags&net.FlagUp != 0 &&
			iface.HardwareAddr != nil &&
			iface.Name == "Wi-Fi" {
			return iface.HardwareAddr.String()
		}
	}

	logger.Error("Failed to get MAC address", nil)
	return ""
}
