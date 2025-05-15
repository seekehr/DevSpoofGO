package cli

import (
	"bufio"
	"errors"
	"fmt"
	"github.com/seekehr/DevSpoofGO/config"
	"github.com/seekehr/DevSpoofGO/injector"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"syscall"
	"time"

	info "github.com/seekehr/DevSpoofGO/info"
	logger "github.com/seekehr/DevSpoofGO/logger"
)

func Start(info *info.Info) {
	logger.Cli("Enter the app you want to inject:")
	app := getInput()
	var pid int
	if strings.HasSuffix(app, ".exe") {
		logger.Info("Listening for " + app)
		for {
			condition, err := listenForApp(app)
			if err != nil {
				logger.Error("Failed to check if app is running", err)
				os.Exit(1)
			}

			if condition {
				logger.Success("Found " + app + " running")
				getPid, _ := getProcessIDByName(app)
				if getPid == 0 {
					logger.Error("Failed to get process ID", nil)
					os.Exit(1)
					return
				}
				pid = getPid
				logger.Success("Found PID for app " + app + ": " + strconv.Itoa(pid))

				// Inject the DLL into the process
				if !cliInject(pid) {
					logger.Error("Failed WHILE injecting DLL", nil)
					os.Exit(1)
					return
				}
				break
			} else {
				logger.Warn("App " + app + " is not currently running")
			}
			time.Sleep(1 * time.Second)
		}
	} else {
		logger.Error("Invalid app", nil)
		os.Exit(1)
	}

	// main loop
	for {
		condition, err := listenForApp(app)
		if !condition || err != nil {
			logger.Warn("App " + app + " is not currently running (re-inject by running `spoof inject`!)")
			time.Sleep(1 * time.Second)
			pid = 0
			continue
		} else if pid == 0 {
			newPid, err := getProcessIDByName(app)
			if newPid > 0 && err == nil {
				pid = newPid
				logger.Success("Found new PID for app " + app + ": " + strconv.Itoa(pid))
				continue
			}

			logger.Error("Failed to get process ID", err)
			os.Exit(1)
			return
		}

		logger.Cli("Enter command: ")
		input := getInput()
		switch {
		case input == "exit":
			logger.Info("Exiting program.")
			os.Exit(0)
		case strings.Contains(input, "info"):
			arg := getArg(input, 1)
			if arg == "" {
				logger.Error("Info command requires an argument", nil)
				continue
			}

			switch arg {
			case "os":
				logger.Info("====OS====")
				os := info.GetOSInfo()
				logger.Log("Hostname: " + os.GetHostname())
				logger.Log("Install Date: " + os.GetInstallDate())
				logger.Log("Serial Number: " + os.GetSerialNumber())
				logger.Log("User SID: " + os.GetUserSID())
				logger.Info("====OS====")
			case "hardware":
				logger.Info("====HARDWARE====")
				hardware := info.GetHardwareInfo()
				logger.Log("Motherboard Serial Number: " + hardware.GetMotherboardSerialNumber())
				logger.Log("BIOS Serial Number: " + hardware.GetBiosSerialNumber())
				logger.Log("Processor ID: " + hardware.GetProcessorID())
				logger.Log("System UUID: " + hardware.GetSystemUUID())
				logger.Log("Machine GUID: " + hardware.GetMachineGUID())
				logger.Info("====HARDWARE====")
			case "network":
				logger.Info("====NETWORK====")
				network := info.GetNetworkInfo()
				logger.Log("Local IP: " + network.GetLocalIP())
				logger.Log("Public IP: " + network.GetPublicIP())
				logger.Log("WLAN GUID: " + network.GetWlanGUID())
				logger.Log("WLAN Physical Address: " + network.GetWlanPhysicalAddress())
				logger.Log("WLAN BSSID: " + network.GetWlanBSSID())
				logger.Info("====NETWORK====")
			case "disk":
				logger.Info("====DISK====")
				disk := info.GetDiskInfo()
				logger.Log("Disk Serial Number: " + disk.GetDiskSerialNumber())
				logger.Log("Disk Model: " + disk.GetDiskModel())
				logger.Log("Volume Serial Number: " + disk.GetVolumeSerialNumber())
				logger.Info("====DISK====")
			default:
				logger.Error("Invalid argument for info", nil)
			}
		case strings.Contains(input, "spoof"):
			arg := getArg(input, 1)
			if arg == "" {
				logger.Error("Spoof command requires an argument", nil)
				continue
			}

			switch arg {
			case "inject":
				if !cliInject(pid) {
					logger.Error("Failed WHILE injecting DLL", nil)
					os.Exit(1)
					return
				}
				break
			default:
				logger.Error("Invalid argument for spoof", nil)
			}
		case input == "backup":
			err := createRegistryBackup()
			if err != nil {
				logger.Error("Failed to create registry backup", err)
			} else {
				logger.Success("Registry backup created successfully")
			}
		default:
			logger.Error("Invalid command", nil)
		}
	}
}

func cliInject(pid int) bool {
	dllHandle, processHandle, err := injector.Inject(pid) // Replace with actual DLL path
	if err != nil {
		logger.Error("Failed to inject DLL", err)
		os.Exit(1)
		return false
	} else {
		logger.Success(fmt.Sprintf("DLL injected successfully. Base address (HMODULE): 0x%X and process address/handle: 0x%X", dllHandle, processHandle))
		dllConfig, err := config.New(syscall.Handle(dllHandle), syscall.Handle(processHandle))
		if err != nil {
			logger.Error("Failed to create DLL config", err)
			os.Exit(1)
			return false
		}

		err = dllConfig.ConfigureDLL()
		if err != nil {
			logger.Error("Failed to configure DLL", err)
			return false
		} else {
			logger.Success("DLL configured successfully")
		}
	}

	return true
}

func listenForApp(app string) (bool, error) {
	// Check if the app is running on Windows
	if os.PathSeparator != '\\' {
		return false, errors.New("this feature is only supported on Windows")
	}

	// Use tasklist to check if the app is running
	cmd := exec.Command("tasklist")
	output, err := cmd.Output()
	if err != nil {
		return false, err
	}

	// Check if the app is in the output
	if strings.Contains(string(output), app) {
		return true, nil
	}

	return false, nil
}

func getInput() string {
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Scan()
	return scanner.Text()
}

func getArg(input string, index int) string {
	parts := strings.Split(input, " ")
	var word string
	if len(parts) > index {
		word = parts[index]
	} else {
		return ""
	}
	return word
}

func createRegistryBackup() error {
	logger.Info("Creating registry backup...")

	// Create a timestamp for the backup file
	timestamp := time.Now().Format("20060102_150405")
	filename := "backup_" + timestamp + ".reg"

	// Define critical registry keys to backup
	registryKeys := []string{
		"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\SystemInformation",
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography",
		"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\IDConfigDB\\Hardware Profiles\\0001",
		"HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\BIOS",
		"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
		"HKEY_LOCAL_MACHINE\\SYSTEM\\HardwareConfig",
	}

	// Create the backup file
	file, err := os.Create(filename)
	if err != nil {
		return err
	}
	defer file.Close()

	// Write registry file header
	_, err = file.WriteString("Windows Registry Editor Version 5.00\n\n")
	if err != nil {
		return err
	}

	// Export each registry key and append to the backup file
	for _, key := range registryKeys {
		logger.Log("Backing up: " + key)

		// Use reg export to get the key content
		cmd := exec.Command("reg", "export", key, "temp.reg", "/y")
		err := cmd.Run()
		if err != nil {
			logger.Warn("Failed to export key: " + key)
			continue
		}

		// Read the exported file
		tempData, err := os.ReadFile("temp.reg")
		if err != nil {
			logger.Warn("Failed to read exported key: " + key)
			continue
		}

		// Skip the header in the temp file (first two lines)
		lines := strings.Split(string(tempData), "\n")
		if len(lines) > 2 {
			keyContent := strings.Join(lines[2:], "\n")
			_, err = file.WriteString(keyContent + "\n")
			if err != nil {
				return err
			}
		}
	}

	// Clean up temp file
	os.Remove("temp.reg")

	logger.Info("Registry backup saved to: " + filename)
	return nil
}

// getProcessIDByName returns the process ID of a process with the given name
func getProcessIDByName(procName string) (int, error) {
	// Use tasklist to get details of the process
	cmd := exec.Command("tasklist", "/FI", "IMAGENAME eq "+procName, "/FO", "CSV", "/NH")
	output, err := cmd.Output()
	if err != nil {
		return 0, err
	}

	// Parse the output to get the PID
	lines := strings.Split(string(output), "\n")
	if len(lines) < 1 || strings.TrimSpace(lines[0]) == "" { // Added check for empty output
		return 0, errors.New("process not found")
	}

	// Remove quotes and split by comma
	parts := strings.Split(strings.Trim(lines[0], "\"\r"), "\",\"")
	if len(parts) < 2 {
		return 0, errors.New("unexpected output format from tasklist")
	}

	// PID is the second column
	pid, err := strconv.Atoi(parts[1])
	if err != nil {
		return 0, err
	}

	return pid, nil
}
