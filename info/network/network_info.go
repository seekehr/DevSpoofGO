package info_network

import (
	"io/ioutil"
	"net"
	"net/http"
	"os/exec"
	"strings"
	"sync"

	logger "github.com/seekehr/DevSpoofGO/logger"
)

type NetworkInfo struct {
	localIP      string
	publicIP     string
	wlanGUID     string
	wlanPhysAddr string
	wlanBSSID    string
	mac          string

	lock sync.Mutex
}

func NewNetworkInfo() *NetworkInfo {
	return &NetworkInfo{
		localIP:      getLocalIP(),
		publicIP:     getPublicIP(),
		wlanGUID:     getWlanGUID(),
		wlanPhysAddr: getWlanPhysicalAddress(),
		wlanBSSID:    getWlanBSSID(),
		mac:          getMacAddress(),
		lock:         sync.Mutex{},
	}
}

func (n *NetworkInfo) GetLocalIP() string {
	return n.localIP
}

func (n *NetworkInfo) GetPublicIP() string {
	return n.publicIP
}

func (n *NetworkInfo) GetWlanGUID() string {
	return n.wlanGUID
}

func (n *NetworkInfo) GetWlanPhysicalAddress() string {
	return n.wlanPhysAddr
}

func (n *NetworkInfo) GetWlanBSSID() string {
	return n.wlanBSSID
}

func (n *NetworkInfo) GetMacAddress() string {
	return n.mac
}

func getLocalIP() string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		logger.Error("Failed to get local IP address", err)
		return ""
	}

	for _, addr := range addrs {
		if ipnet, ok := addr.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				return ipnet.IP.String()
			}
		}
	}

	logger.Error("Failed to find non-loopback IPv4 address", nil)
	return ""
}

func getPublicIP() string {
	resp, err := http.Get("https://api.ipify.org")
	if err != nil {
		logger.Error("Failed to get public IP address", err)
		return ""
	}
	defer resp.Body.Close()

	ip, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		logger.Error("Failed to read public IP response", err)
		return ""
	}

	return string(ip)
}

func getWlanGUID() string {
	cmd := exec.Command("netsh", "wlan", "show", "interfaces")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get WLAN GUID", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "GUID") {
			parts := strings.Split(line, ":")
			if len(parts) >= 2 {
				return strings.TrimSpace(parts[1])
			}
		}
	}

	logger.Error("Failed to parse WLAN GUID", nil)
	return ""
}

func getWlanPhysicalAddress() string {
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

	logger.Error("Failed to get WLAN physical address", nil)
	return ""
}

func getWlanBSSID() string {
	cmd := exec.Command("netsh", "wlan", "show", "interfaces")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get WLAN BSSID", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	for _, line := range lines {
		if strings.Contains(line, "BSSID") {
			parts := strings.Split(line, ":")
			if len(parts) >= 2 {
				return strings.TrimSpace(parts[1])
			}
		}
	}

	logger.Error("Failed to parse WLAN BSSID", nil)
	return ""
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
			len(iface.HardwareAddr) > 0 &&
			!strings.HasPrefix(iface.Name, "vEthernet") &&
			!strings.HasPrefix(iface.Name, "Loopback") {
			return iface.HardwareAddr.String()
		}
	}

	logger.Error("Failed to get MAC address", nil)
	return ""
}
