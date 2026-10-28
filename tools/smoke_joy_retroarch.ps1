param(
    [string]$RetroArchDir='D:/Emulation/Emulators/RetroArch',
    [string]$Content='D:/Emulation/ROMs/X68000/HardBattle.xdf',
    [int]$Frames=180
)
$ErrorActionPreference='Stop'
$xmJoyRepo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$xmJoyOut=Join-Path $PSScriptRoot 'typeg_audio/build/joyfix/retroarch'
$xmJoySystem=Join-Path $xmJoyOut 'system'
$xmJoyContentDir=Join-Path $xmJoyOut 'content'
$xmJoyConfigDir=Join-Path $xmJoyOut 'config'
foreach ($xmJoyDir in @($xmJoyOut,$xmJoySystem,$xmJoyContentDir,$xmJoyConfigDir,(Join-Path $xmJoyOut 'saves'),(Join-Path $xmJoyOut 'states'))) {
    New-Item -ItemType Directory -Force -Path $xmJoyDir | Out-Null
}
$xmJoyResultFile=Join-Path $xmJoyOut 'result.json'
if (Test-Path -LiteralPath $xmJoyResultFile) { Remove-Item -LiteralPath $xmJoyResultFile }
$xmJoyExe=Join-Path $RetroArchDir 'retroarch.exe'
$xmJoySource=Join-Path $xmJoyRepo 'libretro/Release_x64/xm6_libretro.dll'
$xmJoyInstalled=Join-Path $RetroArchDir 'cores/xm6_libretro.dll'
foreach ($xmJoyFile in @($xmJoyExe,$xmJoySource,$Content)) {
    if (!(Test-Path -LiteralPath $xmJoyFile -PathType Leaf)) { throw "Missing: $xmJoyFile" }
}
if (@(Get-Process retroarch -ErrorAction SilentlyContinue).Count) { throw 'RetroArch is already running; do not replace a loaded core.' }
$xmJoyBackup=Join-Path $xmJoyOut ('previous-installed-core-'+[guid]::NewGuid().ToString('N')+'.dll')
if (Test-Path -LiteralPath $xmJoyInstalled) { Copy-Item -LiteralPath $xmJoyInstalled -Destination $xmJoyBackup }
Copy-Item -LiteralPath $xmJoySource -Destination $xmJoyInstalled -Force
if ((Get-FileHash $xmJoySource).Hash -ne (Get-FileHash $xmJoyInstalled).Hash) { throw 'Installed DLL hash mismatch' }
foreach ($xmJoyBios in @('iplrom.dat','cgrom.dat','sram.dat')) {
    Copy-Item -LiteralPath (Join-Path $RetroArchDir "system/$xmJoyBios") -Destination (Join-Path $xmJoySystem $xmJoyBios) -Force
}
$xmJoyDisk=Join-Path $xmJoyContentDir (Split-Path $Content -Leaf)
Copy-Item -LiteralPath $Content -Destination $xmJoyDisk -Force
$xmJoyOptions=Join-Path $xmJoyOut 'core-options.cfg'
@'
xm6_joy1_type = "cpsf_sfc"
xm6_joy2_type = "cpsf_sfc"
xm6_cpu_clock = "10mhz"
xm6_audio_engine = "YMFM"
xm6_render_mode = "original"
xm6_pad_start_select = "disabled"
'@ | Set-Content -LiteralPath $xmJoyOptions -Encoding ASCII
$xmJoyCfg=Join-Path $xmJoyOut 'retroarch-smoke.cfg'
@"
config_save_on_exit = "false"
core_options_path = "$($xmJoyOptions.Replace('\','/'))"
system_directory = "$($xmJoySystem.Replace('\','/'))"
rgui_config_directory = "$($xmJoyConfigDir.Replace('\','/'))"
savefile_directory = "$($xmJoyOut.Replace('\','/'))/saves"
savestate_directory = "$($xmJoyOut.Replace('\','/'))/states"
content_history_dir = "$($xmJoyOut.Replace('\','/'))"
content_history_size = "0"
history_list_enable = "false"
runtime_log_directory = "$($xmJoyOut.Replace('\','/'))/runtime"
content_runtime_log = "false"
content_runtime_log_aggregate = "false"
core_options_global = "true"
auto_overrides_enable = "false"
auto_remaps_enable = "false"
auto_shaders_enable = "false"
savestate_auto_load = "false"
savestate_auto_save = "false"
video_driver = "null"
audio_driver = "null"
audio_enable = "false"
microphone_enable = "false"
midi_driver = "null"
midi_input = "OFF"
midi_output = "OFF"
input_driver = "null"
input_joypad_driver = "null"
input_autodetect_enable = "false"
video_fullscreen = "false"
video_vsync = "false"
audio_sync = "false"
menu_enable_widgets = "false"
log_verbosity = "true"
libretro_log_level = "0"
"@ | Set-Content -LiteralPath $xmJoyCfg -Encoding ASCII
$xmJoyLog=Join-Path $xmJoyOut 'retroarch.log'
$xmJoyArgs=@('-c',('"'+$xmJoyCfg+'"'),'-L',('"'+$xmJoyInstalled+'"'),'-v',('--log-file="'+$xmJoyLog+'"'),("--max-frames=$Frames"),('"'+$xmJoyDisk+'"'))
$xmJoyProcess=Start-Process -FilePath $xmJoyExe -WorkingDirectory $RetroArchDir -ArgumentList $xmJoyArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $xmJoyOut 'stdout.log') -RedirectStandardError (Join-Path $xmJoyOut 'stderr.log')
if (!$xmJoyProcess.WaitForExit(45000)) { Stop-Process -Id $xmJoyProcess.Id;throw 'RetroArch smoke timed out after 45 seconds' }
$xmJoyProcess.WaitForExit()
$xmJoyText=Get-Content -LiteralPath $xmJoyLog -Raw
if ($xmJoyProcess.ExitCode -ne 0) { throw "RetroArch exited with code $($xmJoyProcess.ExitCode); see $xmJoyLog" }
if ($xmJoyText -notmatch 'joy1=7 joy2=7' -or $xmJoyText -notmatch 'clock=10mhz' -or $xmJoyText -notmatch 'Audio runtime: engine=YMFM') { throw 'Expected CPSF-SFC/10MHz/YMFM startup not found in RetroArch log' }
if ($xmJoyText -notmatch 'Unloading game|Unloading core') { throw 'RetroArch did not complete normal content teardown' }
# This installed frontend logs its silence notice as ERROR even when physical
# audio is deliberately disabled. Keep that exact notice in the report and
# reject every other error instead of treating exit code zero alone as success.
$xmJoySilenceNotice='\[ERROR\] Error al iniciar el controlador de audio\. Se continuar[^\r\n]*en silencio\.'
$xmJoyExpectedSilenceCount=([regex]::Matches($xmJoyText,$xmJoySilenceNotice)).Count
$xmJoyUnexpected=[regex]::Replace($xmJoyText,$xmJoySilenceNotice,'')
if ($xmJoyUnexpected -match '\[ERROR\]') { throw 'RetroArch reported an unexpected error; inspect its smoke log' }
$xmJoyReport=@{
    date='2026-10-28';status='PASS';exit_code=$xmJoyProcess.ExitCode;max_frames=$Frames
    installed_core=$xmJoyInstalled;core_sha256=(Get-FileHash $xmJoyInstalled).Hash.ToLowerInvariant()
    backup=$xmJoyBackup;content_original=$Content;content_private_copy=$xmJoyDisk
    log=$xmJoyLog;computer_use=$false
    audio_disabled_notice_count=$xmJoyExpectedSilenceCount
    scope='Actual RetroArch CLI startup/run/normal exit; null video and physical audio disabled; both CPSF-SFC ports. Hot option/button regression is tested separately by test_libretro_joy_options.'
}
$xmJoyReport | ConvertTo-Json | Set-Content -LiteralPath $xmJoyResultFile -Encoding UTF8
$xmJoyReport | ConvertTo-Json
