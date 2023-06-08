@echo off
echo I will fetch required binaries and create user folders.
echo When done, drag the echo UnrealLibretro folder into your project's Plugins folder.
echo Enable "Show Plugin Content", then try the example map in the plugin's content folder.
echo.

REM Make directories that won't be dynamically generated
for %%d in (MyROMs MyCores Binaries\Win64\ThirdParty\libretro) do if not exist %%d md %%d && echo - Created %%d

REM Get general binaries for running Libretro Cores on Windows
set TMPFILE="%TEMP%\RetroArch_update.7z"
if exist %TMPFILE% goto Unpack

REM Prompt user to install curl if necessary (it is a default component on Windows 10 and 11)
where curl >nul 2>nul
if %errorlevel% neq 0 (
    echo curl not found. You can download and install it from https://curl.se/
    exit /b 1
)
echo - Downloading file to %TMPFILE%
call curl --ssl-no-revoke -o %TMPFILE% https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch_update.7z

:Unpack
REM Prompt user to install 7zip if necessary
where 7z >nul 2>nul
if %errorlevel% neq 0 (
    echo 7zip not found. You can download and install it from https://www.7-zip.org/
    exit /b 1
)
echo - Unpacking & copying file
call 7z x -aoa -o"%TEMP%" %TMPFILE%
move /y "%TEMP%\RetroArch-Win64\*.dll" Binaries\Win64\ThirdParty\libretro

echo.
echo - Clean up temporary files
del %TMPFILE%
rd /s /q "%TEMP%\RetroArch-Win64"
