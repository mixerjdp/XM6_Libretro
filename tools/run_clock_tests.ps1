param([string]$Python='C:/Python313/python.exe')
$ErrorActionPreference='Stop'
$xmClockRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$xmClockOutput=Join-Path $PSScriptRoot 'typeg_audio/build'
Push-Location $xmClockRoot
try {
    foreach ($xmClockBits in @('32','64')) {
        & cmd /d /c "`"$PSScriptRoot/build_clock_test.bat`" $xmClockBits"
        if ($LASTEXITCODE -ne 0) { throw "Clock harness build failed: $xmClockBits" }
        $xmClockDll=if ($xmClockBits -eq '32') {'libretro/Release/xm6_libretro.dll'} else {'libretro/Release_x64/xm6_libretro.dll'}
        & (Join-Path $xmClockOutput "test_clock$xmClockBits.exe") (Join-Path $xmClockRoot $xmClockDll) (Join-Path $xmClockOutput 'runtime') > (Join-Path $xmClockOutput "clock_options$xmClockBits.json")
        if ($LASTEXITCODE -ne 0) { throw "Clock option test failed: $xmClockBits" }
        Get-Content (Join-Path $xmClockOutput "clock_options$xmClockBits.json")
    }
    & $Python (Join-Path $PSScriptRoot 'timing_probe.py')
    if ($LASTEXITCODE -ne 0) { throw 'Synthetic CPU throughput test failed' }
    $xmClockHashes=@{}
    foreach ($xmClockFile in @('libretro/Release/xm6_libretro.dll','libretro/Release_x64/xm6_libretro.dll','libretro/xm6_libretro.cpp','tools/test_libretro_clock.cpp','tools/timing_probe.py')) {
        $xmClockHashes[$xmClockFile]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $xmClockRoot $xmClockFile)).Hash.ToLowerInvariant()
    }
    $xmClockReport=@{
        date='2026-10-24';status='PASS';sha256=$xmClockHashes
        baseline='Previous x64 DLL reproduced 10mhz -> scheduler index 6, speed 2500 (25 MHz).'
        options32=(Get-Content (Join-Path $xmClockOutput 'clock_options32.json') -Raw | ConvertFrom-Json)
        options64=(Get-Content (Join-Path $xmClockOutput 'clock_options64.json') -Raw | ConvertFrom-Json)
        throughput=(Get-Content (Join-Path $xmClockOutput 'clock_throughput.json') -Raw | ConvertFrom-Json)
    }
    $xmClockReport | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $PSScriptRoot 'libretro_clock_validation.json')
} finally { Pop-Location }
