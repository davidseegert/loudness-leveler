@echo off
setlocal enabledelayedexpansion

:: build-win.bat - Build and run the Loudness Leveler project on Windows

:: Kill existing instance to prevent file locking
taskkill /F /IM "Loudness Leveler.exe" 2>nul

if not exist build (
    mkdir build
)

cd build

:: Force CMake to reconfigure cleanly to pick up the new triplet
if exist CMakeCache.txt del CMakeCache.txt

:: Run CMake with Release configuration
echo Configuring project (Release mode)...
set "CMAKE_ARGS="
if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo Found vcpkg toolchain, using it.
    set "CMAKE_ARGS=-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows"
)

cmake %CMAKE_ARGS% -DCMAKE_BUILD_TYPE=Release ..
if !ERRORLEVEL! neq 0 (
    echo CMake configuration failed!
    exit /b !ERRORLEVEL!
)

:: Build the project using cmake --build
echo Building project...
cmake --build . --config Release
if !ERRORLEVEL! neq 0 (
    echo Building failed!
    exit /b !ERRORLEVEL!
)

:: If build is successful, package and run the executable
echo Build successful! Packaging executable and DLLs...
if exist "Release\Loudness Leveler.exe" (
    powershell -Command "Compress-Archive -Path 'Release\Loudness Leveler.exe', 'Release\*.dll' -DestinationPath 'Loudness Leveler-win64.zip' -Force"
    echo Packaging complete. Created build\Loudness Leveler-win64.zip.
    start "" "Release\Loudness Leveler.exe"
) else (
    echo Executable not found in Release folder.
    exit /b 1
)
