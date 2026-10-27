# Libretro: 10 MHz selected but 25 MHz applied

2026-10-24. User observed that 10 MHz ran faster than 12 MHz in XM6 Libretro.

Cause: V2 options offered `xm6_cpu_clock=10mhz`, but
`apply_core_option_values()` only recognized 12/16/22/25/40 MHz. `10mhz` fell
through to the default index 6, i.e. 25 MHz. `system_clock_label()` also lacked
index 0. The old frontend API omitted the clock option entirely.

Fix: recognize `10mhz` as scheduler index 0; label index 0 correctly; register
the same six choices for legacy frontends, keeping the existing 25 MHz default.
Invalid/missing values retain that default. No scheduler/audio code changed.

Reproduced against the previous compiled x64 Libretro DLL through the real
environment callback: request `10mhz` produced index 6, speed 2500 instead of
index 0, speed 979. The new regression fails against that DLL.

Validation commands, after compiling both Libretro builds:

```powershell
./tools/run_clock_tests.ps1
```

`tools/test_libretro_clock.cpp` loads the actual DLL and supplies frontend
option callbacks using APIs V2 and legacy. It inspects the resulting saved
scheduler state, checks all 4096 cycle-table entries and the diagnostic clock
label for every offered value plus invalid/missing defaults. A deliberately
missing fixture stops content loading before any guest CPU execution. Results:
`tools/typeg_audio/build/clock_options32.json` and `clock_options64.json`.
Both pass: sixteen option cases per architecture, including both frontend APIs,
each checking the actual 4096-entry cycle table and the diagnostic label.

`tools/timing_probe.py` executes only a synthetic ADDQ.L/BRA.S loop in the
production core, with interrupts masked, at all eight scheduler clocks and
both video modes. It verifies exact elapsed virtual time and instruction
throughput within one loop of the 18-cycle loop budget. This distinguishes
CPU MHz from host wall-clock time or displayed FPS. Result:
`tools/typeg_audio/build/clock_throughput.json`.

For 100 ms of emulated time, 10 MHz executes 54389 loop increments and 12 MHz
executes 65056. Both advance the same 200000 half-microsecond units; the
existing scheduler table uses its historical calibrated 979/1171 constants.
All sixteen synthetic throughput cases pass. The combined report with source
and rebuilt DLL hashes is `tools/libretro_clock_validation.json`. The earlier
TypeG audio manifests describe the builds before this clock-only correction.

No GUI, Computer Use or game execution. Reopen the core in
RetroArch to load the rebuilt DLL; no change of user configuration is required.
