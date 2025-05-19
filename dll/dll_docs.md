# Intercepted functions w/ spoof_dll

## `info/os/os_info`
- GetComputerNameA
- GetComputerNameW

**PC hostname is spoofed.**

## `info/network/wlan_info`
- GetAdaptersInfo
- GetAdaptersAddresses
- WlanEnumInterfaces
- WlanQueryInterface
- WlanGetNetworkBssList

**MAC address, WLAN guid and BSSID are spoofed.**

## `info/disk/vol_info`
- GetVolumeInformationA
- GetVolumeInformationW

**Volume serial is spoofed.**

## `info/hardware/motherboard_serial`
- GetSystemFirmwareTable
- 
**Motherboard serial is spoofed.**