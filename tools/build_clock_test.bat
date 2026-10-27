@echo off
setlocal
set XM_CLOCK_BITS=%1
if "%XM_CLOCK_BITS%"=="" set XM_CLOCK_BITS=64
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars%XM_CLOCK_BITS%.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%~dp0typeg_audio\build\clock%XM_CLOCK_BITS%" mkdir "%~dp0typeg_audio\build\clock%XM_CLOCK_BITS%"
cl /nologo /O2 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /Fo"%~dp0typeg_audio\build\clock%XM_CLOCK_BITS%/" /Fe"%~dp0typeg_audio\build\test_clock%XM_CLOCK_BITS%.exe" "%~dp0test_libretro_clock.cpp"
exit /b %errorlevel%
