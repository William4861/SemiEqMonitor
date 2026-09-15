@echo off
setlocal

set "PROJECT_DIR=%~dp0.."
set "BIN_DIR=%PROJECT_DIR%\build\bin"
set "QT_ROOT=D:\OtherSoftwares\QT\QT\5.14.2\mingw73_64"
set "MINGW_BIN=D:\OtherSoftwares\QT\QT\Tools\mingw730_64\bin"
set "PATH=%QT_ROOT%\bin;%MINGW_BIN%;%PATH%"

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=monitor"

if /i "%TARGET%"=="sim"     goto sim
if /i "%TARGET%"=="monitor" goto monitor
if /i "%TARGET%"=="test"    goto test
goto usage

:sim
if not exist "%BIN_DIR%\SemiEqSimulator.exe" goto notbuilt
echo Starting SemiEqSimulator (port 1502) ...
echo Console commands: h=inject fault  n=recover  d=drop client  l=list  q=quit
echo.
"%BIN_DIR%\SemiEqSimulator.exe" %2 %3
goto end

:monitor
if not exist "%BIN_DIR%\SemiEqMonitor.exe" goto notbuilt
echo Starting SemiEqMonitor ...
"%BIN_DIR%\SemiEqMonitor.exe" %2 %3
goto end

:test
if not exist "%BIN_DIR%\semieq_tests.exe" goto notbuilt
"%BIN_DIR%\semieq_tests.exe" %2 %3
goto end

:notbuilt
echo [FAIL] Executables not found in %BIN_DIR%
echo        Run scripts\build.bat first.
exit /b 1

:usage
echo Usage: run.bat [monitor ^| sim ^| test]
echo.
echo   monitor  Start the HMI application (default)
echo   sim      Start the device simulator (start this one FIRST)
echo   test     Run unit tests
echo.
echo Typical demo:
echo   1) scripts\run.bat sim      (leave it running)
echo   2) scripts\run.bat monitor  (in another window)
exit /b 1

:end
endlocal
