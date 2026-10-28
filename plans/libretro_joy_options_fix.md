# Keep the selected controller across runtime option changes

2026-10-28. CPSF-SFC was selected in Libretro, but changing unrelated core
options replaced it with the default Atari controller in the VM.

`xm6_set_joy_type()` updated the PPI device without updating the corresponding
`runtime_config.joy_type[port]`. Generic option application calls setters such
as CPU clock, RAM and floppy speed, which reapply the complete Config. PPI then
recreated controllers from stale default values 1/0. Libretro still used its
selected types 7/7 to map buttons and did not restore the devices, because the
controller options themselves had not changed.

The fix stores the selected type in runtime_config before calling SetJoyType.
When unrelated settings are reapplied, PPI sees matching types and retains the
existing controller. This also covers nondefault controllers on the second port.
No changes to the intended CPSF-SFC button layout or the PPI protocol were made.

Both Libretro Win32 and x64 were rebuilt. The old x64 binary was retained in
`tools/typeg_audio/build/joyfix/before64.dll` and reproduced the problem on the
first FM volume change: PPI types became 1/0 instead of 7/7.

## Regression

```powershell
./tools/run_joy_options_tests.ps1
```

Uses the actual compiled Libretro DLL, frontend environment/input callbacks,
two CPSF-SFC ports, a temporary blank floppy and a synthetic 68000 loop that
reads the low/high banks of both controller ports into RAM. Each of 24 hot
option changes is followed by tests of all eight buttons. It checks the saved
PPI types and four bytes read by the guest per button, detecting both loss of
controller type and changes to button behavior. Initial firmware boot runs on
the blank floppy; no game runs in this harness.

Result: 192 button cases per architecture, with both ports and both banks
checked in every case. Changes cover volume, CPU clock, floppy timing, raster,
FM backend/rate/filter, reverb, EQ, surround, bass, DMA compatibility, mouse,
MIDI and ADPCM options. Both architectures pass. Report with source/binary
hashes: `tools/joy_options_validation.json`.

The private BIOS runtime used by the regression is prepared with the local
`tools/typeg_audio/prepare_runtime.ps1` tooling, which the user excludes from Git.

## Actual RetroArch smoke

```powershell
./tools/smoke_joy_retroarch.ps1
```

Uses `D:/Emulation/Emulators/RetroArch/retroarch.exe` by CLI, installs the new x64
core in its cores directory and keeps a backup of the previous installed DLL in
the private smoke directory. Copies BIOS/SRAM and HardBattle.xdf into that
directory and uses a private config/options/saves/states location. Video is
headless, physical audio is disabled, and the process has a 45-second timeout.
It runs 180 frames with both ports CPSF-SFC, YMFM and 10 MHz, then exits normally
with code zero. Startup parameters and normal teardown are checked in the log.

This RetroArch build logs its audio-disabled silence notice as ERROR; the smoke
records that exact notice and rejects other errors. Audio hardware and manual
playability are outside this smoke's scope. Hot input changes are verified by
the separate DLL regression above. No Computer Use is used.

Logs and smoke report: `tools/typeg_audio/build/joyfix/retroarch/`.
The original installed core backup from the first run is
`previous-installed-core-fc1331abc9184afdae1a0635ca7043ae.dll` in that directory.
User RetroArch configuration and original game/BIOS files are not edited.
The initial smoke left a normal frontend runtime log; subsequent runs disable
runtime history logging in the private configuration.

The private `tools/typeg_audio` directory remains excluded from publication.
