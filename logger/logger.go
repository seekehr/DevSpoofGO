package logger

import (
	"fmt"
	"os"
)

const (
	redCode    = "\033[31m"
	greenCode  = "\033[32m"
	yellowCode = "\033[33m"
	blueCode   = "\033[34m"
	cyanCode   = "\033[36m"
	reset      = "\033[0m"
)

func isLikelyConsole() bool {
	if os.Getenv("WT_SESSION") != "" {
		return true // Windows Terminal
	}
	if os.Getenv("TERM_PROGRAM") == "vscode" {
		return true // VSCode terminal
	}
	return false
}

var supportsColor = isLikelyConsole()

func Init() {
	fmt.Println("Supports color:", supportsColor)
}

func colorize(colorCode string, message string) string {
	if !supportsColor {
		return message
	}
	return colorCode + message + reset
}

func red(message string) string {
	return colorize(redCode, message)
}

func green(message string) string {
	return colorize(greenCode, message)
}

func yellow(message string) string {
	return colorize(yellowCode, message)
}

func blue(message string) string {
	return colorize(blueCode, message)
}

func cyan(message string) string {
	return colorize(cyanCode, message)
}

func Cli(message string) {
	fmt.Println(cyan("DevSpoofGO> ") + message)
}

func Log(message string) {
	fmt.Println(message)
}

func Error(message string, err error) {
	str := red("[ERROR] ") + message
	if err != nil {
		str += red(" [Exception]: ") + err.Error()
	}
	fmt.Println(str)
}

func Success(message string) {
	fmt.Println(green("[SUCCESS] ") + message)
}

func Info(message string) {
	fmt.Println(blue("[INFO] ") + message)
}

func Warn(message string) {
	fmt.Println(yellow("[WARN] ") + message)
}
