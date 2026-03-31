# XM6_Libretro - Sharp X68000 Emulator

XM6_Libretro is a Sharp X68000 emulator with two main front ends:

- A classic Windows/MFC desktop app
- A Libretro core for RetroArch and similar front ends

The goal of this branch is simple: keep the original XM6 code usable, easier to build, and easier to run on more than one platform.

## Repository layout

- `vm/` - core emulation for X68000 hardware
- `cpu/` - Motorola 68000 CPU core
- `mfc/` - Windows desktop user interface
- `libretro/` - Libretro bridge and core entry points
- `main/` - Visual Studio solution and Windows build files
- `shaders/` - optional video shaders
- `res/` - UI resources and language assets

## Builds

Use the scripts at the repository root:

- `build32.bat` - build the Win32 release
- `build64.bat` - build the Win64 Libretro core
- `main\buildvs.bat` - build the main MFC solution

On Linux, the Libretro core can also be built from `libretro/` with:

```sh
make -f Makefile.libretro -j4
```

## Runtime files

To run correctly, the emulator needs the expected BIOS and system files in place.

Typical X68000 files used by this project include:

- `IPLROM.DAT`
- `CGROM.DAT`
- `CGROM.TMP`
- `SCSIINROM.DAT`
- `SCSIEXROM.DAT`
- `SRAM.DAT`

For RetroArch, the system directory should point to the folder that contains those files.

## Audio

XM6_Libretro exposes FM and ADPCM as separate volume controls.

That makes it easier to tune the mix per backend and per game without forcing a single global sound level.

## Notes

This repository keeps the original XM6 heritage, but the codebase has been updated for modern builds and Libretro support.
