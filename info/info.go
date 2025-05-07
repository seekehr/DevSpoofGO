package info

import (
	"sync"

	info_disk "github.com/seekehr/DevSpoofGO/info/disk"
	info_hardware "github.com/seekehr/DevSpoofGO/info/hardware"
	info_network "github.com/seekehr/DevSpoofGO/info/network"
	info_os "github.com/seekehr/DevSpoofGO/info/os"
)

type Info struct {
	hardware_info *info_hardware.HardwareInfo
	network_info  *info_network.NetworkInfo
	os_info       *info_os.OSInfo
	disk_info     *info_disk.DiskInfo

	lock sync.Mutex
}

func NewInfo() *Info {
	return &Info{
		hardware_info: info_hardware.NewHardwareInfo(),
		network_info:  info_network.NewNetworkInfo(),
		os_info:       info_os.NewOSInfo(),
		disk_info:     info_disk.NewDiskInfo(),
		lock:          sync.Mutex{},
	}
}

func (s *Info) GetInfo() *Info {
	return s
}

func (s *Info) GetHardwareInfo() *info_hardware.HardwareInfo {
	return s.hardware_info
}

func (s *Info) GetNetworkInfo() *info_network.NetworkInfo {
	return s.network_info
}

func (s *Info) GetOSInfo() *info_os.OSInfo {
	return s.os_info
}

func (s *Info) GetDiskInfo() *info_disk.DiskInfo {
	return s.disk_info
}
