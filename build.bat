@echo off
echo =============================================
echo   NetXbox Browser - Build Script
echo =============================================
echo.

if not exist build mkdir build
cd build

echo [1/3] Configuring CMake...
cmake .. -DBUILD_XBOX360=OFF
if %ERRORLEVEL% neq 0 (
    echo ERROR: CMake configure failed!
    pause
    exit /b 1
)

echo [2/3] Building...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo ERROR: Build failed!
    pause
    exit /b 1
)

echo [3/3] Done!
echo.
echo Build output: build\Release\netxbox.exe
echo.

if exist "Release\netxbox.exe" (
    echo Starting NetXbox Browser...
    start "" "Release\netxbox.exe"
) else if exist "netxbox.exe" (
    echo Starting NetXbox Browser...
    start "" "netxbox.exe"
)

cd ..
