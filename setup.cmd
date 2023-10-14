@echo off
setlocal enabledelayedexpansion

REM Add 7zip to the path incase it's not already there
set "PATH=%PATH%;C:\Program Files\7-Zip;C:\Program Filesx86\7-Zip"

REM Make directories that won't be dynamically generated
for %%d in (MyROMs MyCores Binaries\Win64\ThirdParty\libretro) do if not exist %%d md %%d && echo - Created %%d

REM Get general binaries for running Libretro Cores on Windows
set TMPFILE="%TEMP%\RetroArch_update.7z"
if exist %TMPFILE% goto Unpack

REM Prompt user to install curl if necessary (it is a default component on Windows 10 and 11)
where curl >nul 2>nul
if %errorlevel% neq 0 (
    echo curl not found. You can download and install it from https://curl.se/
    pause
    exit /b 1
)
echo - Downloading file to %TMPFILE%
call curl --ssl-no-revoke -o %TMPFILE% https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch_update.7z

:Unpack
REM Prompt user to install 7zip if necessary
where 7z >nul 2>nul
if %errorlevel% neq 0 (
    echo 7zip not found. You can download and install it from https://www.7-zip.org/
    pause
    exit /b 1
)
echo - Unpacking & copying file
call 7z x -aoa -o"%TEMP%" %TMPFILE%
move /y "%TEMP%\RetroArch-Win64\*.dll" Binaries\Win64\ThirdParty\libretro

REM Detect .uproject version
for /R ../../ %%f in (*.uproject) do (
    set "uproject=%%f"
    goto founduproject
)

:defaulttoUE5.3
copy "UnrealLibretro.uplugin.UE5.3" "UnrealLibretro.uplugin"
exit /b

:founduproject
echo Found .uproject: !uproject!

for /f "tokens=2 delims=:" %%a in ('findstr "EngineAssociation" "!uproject!"') do set "version=%%a"
set version=!version:~2,-2!
for /f "tokens=1,2 delims=." %%a in ("!version!") do (
    set major=%%a
    set minor=%%b
)

echo Detected Engine Version: !major!.!minor!

if !major! == 4 copy "UnrealLibretro.uplugin.UE5.2" "UnrealLibretro.uplugin"
if !major! == 5 if !minor! LEQ 2 copy "UnrealLibretro.uplugin.UE5.2" "UnrealLibretro.uplugin"
if !major! == 5 if !minor! GEQ 3 copy "UnrealLibretro.uplugin.UE5.3" "UnrealLibretro.uplugin"

echo Success^^! Now in the Unreal Engine Editor make sure you enable 'Show Plugin Content', then open the example map in the plugin's content folder
