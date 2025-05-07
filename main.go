package main

import (
	cli "github.com/seekehr/DevSpoofGO/cli"
	logger "github.com/seekehr/DevSpoofGO/logger"
)

func main() {
	logger.Info("Starting program.")
	cli.Start()
}
