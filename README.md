# DevSpoofGO (x64)
A device spoofer that injects your desired application with the spoofed credentials, made using C++ (dll) and GoLang (injector).

**LOC:** 7526 (5900 for main project + 1626 for DevSpoofGOTest). Note that LOC does NOT mean better code, or is a measure of how good the project is; I simply want to feel better about giving myself carpel tunnel. Simplicity is key.

Sister repo (for testing): https://github.com/seekehr/DevSpoofGOTest
## How It Works
This project works by injecting a custom DLL into the target process. The DLL uses Microsoft's Detours library to hook key Windows API functions like `GetComputerNameW` and `GetVolumeInformationA`. When the target app tries to ask the system for things like the computer name or volume serial number, we quietly intercept the call and return our own values instead.

All configuration, like what name or serial to spoof, is passed in at runtime from the injector, written in Go. It uses `VirtualAllocEx`, `WriteProcessMemory`, `CreateRemoteThread`, etc to load the DLL and call its setup functions directly in the target process.

As such ;3, the app believes it's getting real system info, but it's actually getting the values we've injected. No file edits, no system changes; just an in-memory override.

## C++?

Within the `dll/` folder only. I'd create a separate repo for it but I don't want to repo spam my poor git profile D;, plus it's more convenient to work like this. A pre-generated spoof_dll.dll exists but you can build it using CMake:

`cmake --build .`

# TODO:
- [ ] Clean up debugging calls (im too tired of this project 😭🙏🏿)
