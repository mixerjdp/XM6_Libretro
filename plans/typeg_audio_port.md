# TypeG ADPCM and sound pages

Requested: replace ADPCM for both MFC and Libretro with TypeG behavior, preserve
existing FM implementations, reproduce Sound/SoundUnit resources and settings,
read previous XM62026 savestates. No commits or pushes. No subagents.

Reference: I:/juegos/xm6s/typegx64/xm6g.exe, SHA256
3d50aab23ce6c8ae1677b1ca8adaa8b0c6c9dabb1c06b98d0f2ea10fe44ac5de.
Existing exports/reconstruction in that workspace are the analysis authority.

Original routines: decode 1404b1860 (859 bytes), encode 1404b1bc0 (424),
filter 1404b2860 (319), resample 1404b1e30 (1296), reset 1404b1d70 (185),
tables 1404b23a0 (1216), callback 1404b1660 (385), clock 1404b17f0 (111),
commands 1404b14a0 (372), configuration 1404b1340 (149).
English dialogs RT_DIALOG 507/508: 210x166 dialog units, MS Sans Serif 8pt.

Implementation steps:
1. Portable integer DSP/state, derived from complete original instructions.
2. Differential isolated tests against original x64 instructions in Unicorn;
   compare tables, PCM and full affected state, not only listening tests.
3. Integrate event/DMA/recording, frame ring and versioned state in ADPCM.
4. Route every Libretro backend through this ADPCM; remove old ADPCM shaping.
5. MFC Sound/SoundUnit, real settings/INI, input and background output.
6. Add Libretro controls, preserve old configuration/state inputs, compile MFC
   Win32 and core builds; document actual verification and any unresolved work.

Important: old state struct is 23 four-byte fields (92 bytes). Keep its reader;
new states must include ring and filter/encoder history for exact continuation.
Old TypeG init uses rate/100, old XM62026 rate/50. New ring uses frame indexes.
Current TypeG commands start only for (command&7)==2 or 4; other commands stop
with delayed decay, status is 0x40 playing / 0xc0 stopped.

Implemented and compiled: steps 1-6. See tools/typeg_audio/README.md and
tools/typeg_audio/reports/validation.json for the exact verification scope and
source/binary hashes. All five Release builds pass. No commits or pushes.

DSP comparisons: 3478 groups plus 25 original host-capture cases. Native x86/x64
each pass 67 original-instruction DSP corpus cases and 15 state cases. Scheduled
core integration passes 25 playback/record streams across five FM backends.
Core option/state tests pass 35 cases; all six compiled page resources and 48
help-control mappings match the reference.

Manual GUI, microphone and listening validation remain unperformed: user
explicitly prohibited Computer Use. No game/CPU execution was part of tests.
