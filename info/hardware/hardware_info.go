package info_hardware

import (
	"os/exec"
	"regexp"
	"strings"
	"sync"

	logger "github.com/seekehr/DevSpoofGO/logger"
)

type HardwareInfo struct {
	motherboardSerialNumber string
	biosSerialNumber        string
	processorID             string
	systemUUID              string
	machineGUID             string
	computerPropertiesCount int

	lock sync.Mutex
}

func NewHardwareInfo() *HardwareInfo {
	return &HardwareInfo{
		motherboardSerialNumber: getMotherboardSerialNumber(),
		biosSerialNumber:        getBiosSerialNumber(),
		processorID:             getProcessorID(),
		systemUUID:              getSystemUUID(),
		machineGUID:             getMachineGUID(),
		computerPropertiesCount: getComputerSystemPropertiesCount(),
		lock:                    sync.Mutex{},
	}
}

func (h *HardwareInfo) Update() {
	h.motherboardSerialNumber = getMotherboardSerialNumber()
	h.biosSerialNumber = getBiosSerialNumber()
	h.processorID = getProcessorID()
	h.systemUUID = getSystemUUID()
	h.machineGUID = getMachineGUID()
}

func (h *HardwareInfo) GetMotherboardSerialNumber() string {
	return h.motherboardSerialNumber
}

func (h *HardwareInfo) GetBiosSerialNumber() string {
	return h.biosSerialNumber
}

func (h *HardwareInfo) GetProcessorID() string {
	return h.processorID
}

func (h *HardwareInfo) GetSystemUUID() string {
	return h.systemUUID
}

func (h *HardwareInfo) GetMachineGUID() string {
	return h.machineGUID
}

func (h *HardwareInfo) GetComputerSystemPropertiesCount() int {
	return h.computerPropertiesCount
}

func getMotherboardSerialNumber() string {
	cmd := exec.Command("wmic", "baseboard", "get", "serialnumber")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get motherboard serial number", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse motherboard serial number", nil)
	return ""
}

func getBiosSerialNumber() string {
	cmd := exec.Command("wmic", "bios", "get", "serialnumber")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get BIOS serial number", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse BIOS serial number", nil)
	return ""
}

func getProcessorID() string {
	cmd := exec.Command("wmic", "cpu", "get", "processorid")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get processor ID", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse processor ID", nil)
	return ""
}

func getSystemUUID() string {
	cmd := exec.Command("wmic", "csproduct", "get", "uuid")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get system UUID", err)
		return ""
	}

	lines := strings.Split(string(output), "\n")
	if len(lines) >= 2 {
		return strings.TrimSpace(lines[1])
	}

	logger.Error("Failed to parse system UUID", nil)
	return ""
}

func getMachineGUID() string {
	// The machine GUID is stored in the registry
	cmd := exec.Command("reg", "query", "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography", "/v", "MachineGuid")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get machine GUID", err)
		return ""
	}

	// Parse the registry output
	// Expected format: MachineGuid    REG_SZ    <GUID>
	outputStr := string(output)
	re := regexp.MustCompile(`MachineGuid\s+REG_SZ\s+([0-9a-fA-F-]+)`)
	matches := re.FindStringSubmatch(outputStr)
	if len(matches) >= 2 {
		return matches[1]
	}

	logger.Error("Failed to parse machine GUID", nil)
	return ""
}

func getComputerSystemPropertiesCount() int {
	cmd := exec.Command("wmic", "computersystem", "list", "full")
	output, err := cmd.Output()
	if err != nil {
		logger.Error("Failed to get computer system properties", err)
		return 0
	}

	lines := strings.Split(string(output), "\n")
	// Count non-empty lines that contain an equals sign (property=value format)
	count := 0
	for _, line := range lines {
		if strings.Contains(line, "=") {
			count++
		}
	}

	if count == 0 {
		logger.Error("Failed to count computer system properties", nil)
	}

	return count
}
