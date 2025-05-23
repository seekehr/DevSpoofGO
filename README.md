# DevSpoofGO (x64)
A project that injects your desired application with the spoofed credentials, made using C++ (dll) and GoLang (injector).

**LOC:** 7526 (5900 for main project + 1626 for DevSpoofGOTest). Note that more LOC do NOT mean better code, and is not a measure of how good the project is; I simply want to feel better about giving myself carpel tunnel. Simplicity is key.

Sister repo (for testing): https://github.com/seekehr/DevSpoofGOTest
## How It Works
This project works by injecting a custom DLL into the target process, by using Microsoft's Detours library to hook key Windows API functions like `GetComputerNameW` and `GetVolumeInformationA`. When the target app tries to ask the system for things like the computer name or volume serial number, we intercept the call and return our own values instead.

All configuration, like custom information for what computer name or bios serial, etc., is passed in at runtime from the injector, written in Go. It uses `VirtualAllocEx`, `WriteProcessMemory`, `CreateRemoteThread`, etc to load the DLL and call its setup functions directly in the target process.

As such, the app believes it's getting real system info, but it's actually getting the values we've injected. No file edits, no system changes; just an in-memory override. The project supports: **common native winapi functions (such as GetSystemFirmwareTable), commonly used registry values (such as MachineGuid and the system certificates) and commonly used WMI values (such as bios serial or processor id).**

## C++?

For the `dll/` only. You can build it by navigating to the `dll/build/` folder, and running:

`cmake --build .`

## Why not just use C++ for the whole thing?

I wanted to get more familiar with Golang and the project sounded fun (initially, at least), so yeah. 

# TODO:
- [ ] Clean up debugging calls (im too tired of this project 😭🙏🏿)
- [ ] Remove dependency on the entire info/ folder; originally the project was made with different intent so the code there still persists, but now it can be removed in favor of DevSpoofGOTest.
- [ ] CLI commands are pretty useless atm
