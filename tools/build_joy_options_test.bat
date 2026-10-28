@echo off
setlocal
set XM_JOY_BITS=%1
if "%XM_JOY_BITS%"=="" set XM_JOY_BITS=64
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars%XM_JOY_BITS%.bat" >nul
if errorlevel 1 exit /b 1
if not exist "%~dp0typeg_audio\build\joyfix\test%XM_JOY_BITS%" mkdir "%~dp0typeg_audio\build\joyfix\test%XM_JOY_BITS%"
cl /nologo /O2 /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /Fo"%~dp0typeg_audio\build\joyfix\test%XM_JOY_BITS%/" /Fe"%~dp0typeg_audio\build\joyfix\test_joy%XM_JOY_BITS%.exe" "%~dp0test_libretro_joy_options.cpp"
exit /b %errorlevel%
