# Intercepted functions w/ spoof_dll

## OS

### `info/os/os_info`
- GetComputerNameA
- GetComputerNameW

**PC hostname is spoofed.**

## Network

### `info/network/wlan_info`
- GetAdaptersInfo
- GetAdaptersAddresses
- WlanEnumInterfaces
- WlanQueryInterface
- WlanGetNetworkBssList

**MAC address, WLAN guid and BSSID are spoofed.**

## Disk

### `info/disk/vol_info`
- GetVolumeInformationA
- GetVolumeInformationW

**Volume serial is spoofed.**

## Hardware

### `info/hardware/motherboard_serial`
- GetSystemFirmwareTable

**Motherboard serial is spoofed.**

### `info/hardware/bios_serial`
- GetSystemFirmwareTable

**BIOS serial is spoofed.**