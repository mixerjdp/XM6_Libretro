$ErrorActionPreference='Stop'
$xmJoyRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$xmJoyOutput=Join-Path $PSScriptRoot 'typeg_audio/build/joyfix'
$xmJoyResults=@{}
Push-Location $xmJoyRoot
try {
    foreach ($xmJoyBits in @('32','64')) {
        & cmd /d /c "`"$PSScriptRoot/build_joy_options_test.bat`" $xmJoyBits"
        if ($LASTEXITCODE -ne 0) { throw "Controller harness build failed: $xmJoyBits" }
        $xmJoyDll=if ($xmJoyBits -eq '32') {'libretro/Release/xm6_libretro.dll'} else {'libretro/Release_x64/xm6_libretro.dll'}
        $xmJoyLog=Join-Path $xmJoyOutput "fixed$xmJoyBits.log"
        & (Join-Path $xmJoyOutput "test_joy$xmJoyBits.exe") (Join-Path $xmJoyRoot $xmJoyDll) (Join-Path $PSScriptRoot 'typeg_audio/build/runtime') *> $xmJoyLog
        if ($LASTEXITCODE -ne 0) { throw "Controller regression failed: $xmJoyBits; see $xmJoyLog" }
        $xmJoyJson=Get-Content -LiteralPath $xmJoyLog | Where-Object {$_ -match '^\{"status":'} | Select-Object -Last 1
        if (!$xmJoyJson) { throw 'Missing controller test result' }
        $xmJoyResults[$xmJoyBits]=$xmJoyJson | ConvertFrom-Json
        Write-Output $xmJoyJson
    }
    $xmJoyHashes=@{}
    foreach ($xmJoyFile in @('vm/xm6core_runtime.cpp','libretro/xm6_libretro.cpp','libretro/Release/xm6_libretro.dll','libretro/Release_x64/xm6_libretro.dll','tools/test_libretro_joy_options.cpp')) {
        $xmJoyHashes[$xmJoyFile]=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $xmJoyRoot $xmJoyFile)).Hash.ToLowerInvariant()
    }
    @{
        date='2026-10-28';status='PASS';tests=$xmJoyResults;sha256=$xmJoyHashes
        baseline='Previous x64 DLL failed after changing FM volume: actual PPI types 1/0 instead of selected CPSF-SFC 7/7.'
        scope='Actual Libretro option-update/input callbacks, both PPI types and guest-observed CPSF-SFC packet bytes; 24 hot option changes, eight buttons, both ports, x86 and x64.'
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'joy_options_validation.json') -Encoding UTF8
} finally { Pop-Location }
