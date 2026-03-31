# XM6_Libretro - X68000 Emulator

XM6_Libretro is a Sharp X68000 emulator with two main build targets:

- A classic Windows/MFC desktop version
- A Libretro core for RetroArch and similar frontends

This branch keeps the emulator practical to build, easier to maintain, and usable on more than one platform.

## What is in this repository

- `vm/` contains the emulation core for the virtual X68000 hardware
- `mfc/` contains the Windows desktop UI
- `libretro/` contains the Libretro frontend/core bridge
- `main/` contains the Windows solution and core project files
- `cpu/` contains the Motorola 68000 CPU core
- `shaders/` contains optional video shaders

## Builds

You can build the project in a few ways:

- `build32.bat` builds the Windows Win32 release
- `build64.bat` builds the Libretro Win64 release
- `main\buildvs.bat` builds the main XM6 Win32 solution

If you are working from Linux, the Libretro core can also be built with the `Makefile.libretro` inside `libretro/`.

## Audio

The project supports separate FM and ADPCM volume controls. The Libretro frontend uses a fixed master volume and exposes the channel controls directly.

## Notes

This repository keeps the original XM6 heritage while adding modern build flows and Libretro support.
