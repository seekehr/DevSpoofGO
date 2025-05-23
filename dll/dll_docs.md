# Intercepted functions w/ spoof_dll

# **Native**

**'Native' spoofs are meant to spoof windows functions that are directly called (e.g `GetComputerNameW`)**

## OS

### `native/os/os_info`
- Hooked_GetComputerNameA
- Hooked_GetComputerNameW

**PC hostname is spoofed by hooking `GetComputerNameA` and `GetComputerNameW`.**

## Network

### `native/network/wlan_info`
- Hooked_GetAdaptersInfo
- Hooked_GetAdaptersAddresses
- Hooked_WlanEnumInterfaces
- Hooked_WlanQueryInterface
- Hooked_WlanGetNetworkBssList

**MAC address, WLAN GUID, BSSID, and related network adapter information are spoofed, and a single adapter is returned for each.**

## Disk

### `native/disk/vol_info`
- Hooked_GetVolumeInformationA
- Hooked_GetVolumeInformationW

**Volume serial is spoofed by hooking `GetVolumeInformationA` and `GetVolumeInformationW`.**

### `native/disk/disk_serial`
- HookedDeviceIoControlForDiskSerial

**Physical disk serial (from `DeviceIoControl` with `IOCTL_STORAGE_QUERY_PROPERTY`) is spoofed.**

## Hardware

### `native/hardware/hardware_info` (Central SMBIOS Hooking)
- Hooked_GetSystemFirmwareTable_Central
- InitializeHardwareHooks
- RemoveHardwareHooks
- GetRealGetSystemFirmwareTableAddressStore
- GetCentralHookFunctionAddress

**The `GetSystemFirmwareTable` function is hooked to enable spoofing of SMBIOS data.**

### `native/hardware/motherboard_serial`
- ModifySmbiosForMotherboardSerial
- InitializeMotherboardSerialHooks

**Motherboard serial is spoofed via data from `hardware_info`.**

### `native/hardware/bios_serial`
- ModifySmbiosForBiosSerial
- InitializeBiosSerialHooks

**BIOS serial is spoofed via via data from `hardware_info`.**

### `native/hardware/processor_id`
- ModifySmbiosForProcessorId
- InitializeProcessorIdHooks

**Processor ID is spoofed via SMBIOS.**

### `native/hardware/system_uuid`
- ModifySmbiosForSystemUuid
- InitializeSystemUuidHooks

**System UUID is spoofed via SMBIOS.**

# **Registry**

**Since spoofing the registry values (permanently) would be slow (due to requiring a system restart likely), we simply hook our custom registry methods to the program, such as `RegGetValueW`, and when the program asks to, let's say, open the MachineGuid key, instead of changing the actual value we simply return our own value.**

### `native/reg/reg_info` (Central Registry Hooking)
- Hooked_RegOpenKeyExW
- Hooked_RegEnumKeyExW
- Hooked_RegQueryValueExW
- Hooked_RegGetValueW
- Hooked_RegCloseKey
- Hooked_RegOpenKeyExA
- Hooked_RegEnumKeyExA
- Hooked_RegQueryValueExA
- Hooked_RegGetValueA
- InitializeRegistryHooksAndHandlers
- RemoveRegistryHooksAndHandlers

**Core registry functions are hooked to enable spoofing for specific registry paths/values by other modules. Reg_info allows the other registry handlers to check if they can handle the call, and if so (e.g if MachineGuid is being queried), return the spoofed value. It also serves as a way to convert ANSI calls (e.g RegQueryValueExA) into W to reduce boilerplate code.**

### `native/reg/current_version`
- Handle_RegOpenKeyExW_ForCurrentVersion
- Handle_RegQueryValueExW_ForCurrentVersion
- Handle_RegGetValueW_ForCurrentVersion
- Handle_RegCloseKey_ForCurrentVersion
- InitializeCurrentVersionSpoofing_Centralized

**OS CurrentVersion registry values (like ProductId) are spoofed.**

### `native/reg/machine_guid`
- Handle_RegQueryValueExW_ForMachineGuid
- Handle_RegGetValueW_ForMachineGuid
- InitializeMachineGuid_Centralized

**MachineGuid (registry) is spoofed.**

### `native/reg/certificates_info`
- Handle_RegOpenKeyExW_ForCertificates
- Handle_RegEnumKeyExW_ForCertificates
- Handle_RegQueryValueExW_ForCertificates
- Handle_RegCloseKey_ForCertificates
- InitializeCertificateSpoofing_Centralized

**Registry-based certificate information is spoofed (Microsoft\Windows NT\CurrentVersion).**


# **WMI**

**Programs often use a wrapper like WMI to grab system info. As such, we hook our own custom functions (yada yada, same concept as the previous 2) into the WMI functions and, bam.**

### `wmi/wmi_handler`
- ExecQuery
- CoCreateInstance
- ConnectServer
- CreateInstanceEnum
- GetObject

**Handles WMI calls (like `ExecQuery`) and redirects them to the appropriate handler (e.g `bios_serial_wmi`).**

### `wmi/wmi_utils`
- Implements `MockClientSecurity` (custom `IClientSecurity`).
- Implements `MockConnectionPointContainer` (custom `IConnectionPointContainer`).
- `MockWbemObject` (base class for custom `IWbemClassObject` implementations, used to create spoofed WMI objects).
- `MockEnumWbemObject` (base class for custom `IEnumWbemClassObject` implementations, used to enumerate spoofed WMI objects).

**Provides utility classes and type definitions for WMI spoofing, and most importantly, base classes for mock WMI objects and enumerators to reduce boilerplate/duplicate code (which is still an issue TBF).**

### `wmi/bios_serial_wmi`
- InitializeBiosSerialWMIModule
- CleanupBiosSerialWMIModule
- Handle_ExecQuery_BiosSerial
- Handle_GetObject_BiosSerial
- Handle_CreateInstanceEnum_BiosSerial

**BIOS serial is spoofed through WMI.**

### `wmi/wmi_physical_memory`
- InitializePhysicalMemoryWMIModule
- CleanupPhysicalMemoryWMIModule
- Handle_ExecQuery_PhysicalMemory
- Handle_GetObject_PhysicalMemory
- Handle_CreateInstanceEnum_PhysicalMemory

**Physical memory SerialNumber and PartNumber are spoofed through WMI (Win32_PhysicalMemory).**

### `wmi/wmi_processor`
- InitializeProcessorWMIModule
- CleanupProcessorWMIModule
- Handle_ExecQuery_Processor
- Handle_GetObject_Processor
- Handle_CreateInstanceEnum_Processor

**Processor SerialNumber and ProcessorId are spoofed through WMI (Win32_Processor).**
