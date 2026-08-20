@echo off
setlocal EnableExtensions

set "ROOT=%~dp0.."
set "VERSION="
for /f "usebackq delims=" %%v in ("%ROOT%\VERSION") do set "VERSION=%%v"
if not defined VERSION set "VERSION=v1.0.0"

set "OUTDIR=%ROOT%\releases"
if exist "%OUTDIR%" rmdir /s /q "%OUTDIR%"
mkdir "%OUTDIR%"

echo [LiteZones] Packaging %VERSION% ...

rem Build x64
call "%~dp0build.cmd" Release x64
if errorlevel 1 goto failed

rem Build ARM64
call "%~dp0build.cmd" Release ARM64
if errorlevel 1 goto failed

rem Package x64
echo [LiteZones] Creating x64 zip ...
mkdir "%OUTDIR%\LiteZones-%VERSION%-x64"
copy "%ROOT%\bin\x64\Release\LiteZones.exe" "%OUTDIR%\LiteZones-%VERSION%-x64\" >nul
call :write_readme "%OUTDIR%\LiteZones-%VERSION%-x64\README.txt"
tar -a -c -f "%OUTDIR%\LiteZones-%VERSION%-x64.zip" -C "%OUTDIR%" "LiteZones-%VERSION%-x64"
if errorlevel 1 goto failed
rmdir /s /q "%OUTDIR%\LiteZones-%VERSION%-x64"

rem Package ARM64
echo [LiteZones] Creating ARM64 zip ...
mkdir "%OUTDIR%\LiteZones-%VERSION%-ARM64"
copy "%ROOT%\bin\ARM64\Release\LiteZones.exe" "%OUTDIR%\LiteZones-%VERSION%-ARM64\" >nul
call :write_readme "%OUTDIR%\LiteZones-%VERSION%-ARM64\README.txt"
tar -a -c -f "%OUTDIR%\LiteZones-%VERSION%-ARM64.zip" -C "%OUTDIR%" "LiteZones-%VERSION%-ARM64"
if errorlevel 1 goto failed
rmdir /s /q "%OUTDIR%\LiteZones-%VERSION%-ARM64"

echo.
echo [LiteZones] Done! Zip files:
echo   %OUTDIR%\LiteZones-%VERSION%-x64.zip
echo   %OUTDIR%\LiteZones-%VERSION%-ARM64.zip
exit /b 0

:failed
echo [LiteZones] Build failed.
exit /b 1

:write_readme
echo LiteZones %VERSION%>"%~1"
echo =================>>"%~1"
echo.>>"%~1"
echo Zone-snapping tool for Windows 10/11.>>"%~1"
echo.>>"%~1"
echo USAGE>>"%~1"
echo   Double-click LiteZones.exe to start.>>"%~1"
echo   Right-click the tray icon to access settings and the layout editor.>>"%~1"
echo.>>"%~1"
echo HOTKEYS>>"%~1"
echo   Shift + drag    Snap window to zone>>"%~1"
echo   Drag a snapped window    Unsnap it (moves freely again)>>"%~1"
echo   Shift + Ctrl + drag (or middle-click + drag)    Span multiple zones>>"%~1"
echo.>>"%~1"
echo REQUIREMENTS>>"%~1"
echo   Windows 10 1809+ (x64) or Windows 11 (x64/ARM64)>>"%~1"
echo.>>"%~1"
echo LICENSE>>"%~1"
echo   MIT License. See https://github.com/hmwassim/LiteZones>>"%~1"
exit /b 0
