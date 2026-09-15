@echo off
setlocal

set "PROJECT_DIR=%~dp0.."
set "QT_ROOT=D:\OtherSoftwares\QT\QT\5.14.2\mingw73_64"
set "MINGW_BIN=D:\OtherSoftwares\QT\QT\Tools\mingw730_64\bin"
set "NINJA=D:\code_work\Python3.13.7\Scripts\ninja.exe"
set "PATH=%QT_ROOT%\bin;%MINGW_BIN%;%PATH%"

echo [1/2] Configuring CMake...
cmake -S "%PROJECT_DIR%" -B "%PROJECT_DIR%\build" -G Ninja ^
      -DCMAKE_BUILD_TYPE=Debug ^
      -DCMAKE_PREFIX_PATH="%QT_ROOT%" ^
      -DCMAKE_MAKE_PROGRAM="%NINJA%"
if errorlevel 1 goto error

echo.
echo [2/2] Building...
cmake --build "%PROJECT_DIR%\build"
if errorlevel 1 goto error

echo.
echo [OK] Build succeeded.
echo      Artifacts: %PROJECT_DIR%\build\bin
exit /b 0

:error
echo.
echo [FAIL] Build failed. See output above.
exit /b 1
