package logger

import (
	"fmt"

	"github.com/fatih/color"
)

var red = color.New(color.FgRed).SprintFunc()
var green = color.New(color.FgGreen).SprintFunc()
var blue = color.New(color.FgBlue).SprintFunc()
var yellow = color.New(color.FgYellow).SprintFunc()
var cyan = color.New(color.FgCyan).SprintFunc()

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
