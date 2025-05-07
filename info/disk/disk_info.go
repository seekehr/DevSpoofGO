package info_disk

import (
	"os/exec"
	"strings"
	"sync"

	logger "github.com/seekehr/DevSpoofGO/logger"
)

type DiskInfo struct {
	diskSerialNumber   string
	diskModel          string
	volumeSerialNumber string

	lock sync.Mutex
}

func NewDiskInfo() *DiskInfo {
	return &DiskInfo{
		diskSerialNumber:   getDiskSerialNumber(),
		diskModel:          getDiskModel(),
		volumeSerialNumber: getVolumeSerialNumber(),
		lock:               sync.Mutex{},
	}
}

func (d *DiskInfo) GetDiskSerialNumber() string {
	return d.diskSerialNumber
}

func (d *DiskInfo) GetDiskModel() string {
	return d.diskModel
}

func (d *DiskInfo) GetVolumeSerialNumber() string {
	return d.volumeSerialNumber
}

func getDiskSerialNumber() string {
	cmd := exec.Command("wmic", "diskdrive", "get", "serialnumber")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get disk serial number", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse disk serial number", nil)
	return ""
}

func getDiskModel() string {
	cmd := exec.Command("wmic", "diskdrive", "get", "model")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get disk model", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse disk model", nil)
	return ""
}

func getVolumeSerialNumber() string {
	// Get the C: drive volume serial number
	cmd := exec.Command("cmd", "/c", "vol", "C:")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get volume serial number", err)
		return ""
	}

	outputStr := string(output)
	// Parse output like "Volume in drive C has no label.\n Volume Serial Number is XXXX-XXXX"
	if strings.Contains(outputStr, "Volume Serial Number is") {
		parts := strings.Split(outputStr, "Volume Serial Number is")
		if len(parts) > 1 {
			return strings.TrimSpace(parts[1])
		}
	}

	logger.Error("Failed to parse volume serial number", nil)
	return ""
}
