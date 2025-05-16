# DevSpoofGO (x64)
A device spoofer that injects your desired application with the spoofed credentials, made using GoLang.

Sister repo (for testing): https://github.com/seekehr/DevSpoofGOTest
## How It Works
This project works by injecting a custom DLL into the target process. The DLL uses Microsoft's Detours library to hook key Windows API functions like `GetComputerNameW` and `GetVolumeInformationA`. When the target app tries to ask the system for things like the computer name or volume serial number, we quietly intercept the call and return our own values instead.

All configuration — like what name or serial to spoof — is passed in at runtime from the injector, written in Go. It uses `VirtualAllocEx`, `WriteProcessMemory`, `CreateRemoteThread`, etc to load the DLL and call its setup functions directly in the target process.

As such ;3, the app believes it's getting real system info, but it's actually getting the values we've injected. No file edits, no system changes; just an in-memory override.

## C++?

Within the `dll/` folder only. I'd create a separate repo for it but I don't want to repo spam my poor git profile D;, plus it's more convenient to work like this. A pre-generated spoof_dll.dll exists but you can re-generate with (in the Developer Command Prompt for VS):

`cl spoof_dll.cpp /LD /I detours\include /link detours\lib.X64\detours.lib`

# TODO:
- [ ] Intercept WMI calls too
- [ ] Update registry values too perhaps (for basic things like pc name at least) and then automatically load the backed up registry once program is terminated for a more seamless experience
- [ ] Update DLL to include more information (like motherboard serial)
- [ ] Update DLL to also include other methods like `GetComputerNameA`
