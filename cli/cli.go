package cli

import (
	"bufio"
	"errors"
	"os"
	"os/exec"
	"strings"
	"time"

	logger "github.com/seekehr/DevSpoofGO/logger"
)

func Start() {
	logger.Cli("Enter the app you want to inject:")
	app := getInput()
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
		logger.Cli("Enter command: ")
		input := getInput()
		if input == "exit" {
			logger.Info("Exiting program.")
			os.Exit(0)
		}
	}
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
