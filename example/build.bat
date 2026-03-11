@echo off
echo ====================================================================
echo VolkDMA Example - Build Script
echo Real Hardware Testing Program
echo ====================================================================
echo.

REM Check if Visual Studio is available
where cl >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Visual Studio compiler not found!
    echo Please run this script from "Developer Command Prompt for VS 2022"
    pause
    exit /b 1
)

echo [INFO] Visual Studio compiler found!
echo.

REM Check if library exists
if not exist "..\x64\Release\VolkDMA.lib" (
    echo [ERROR] VolkDMA.lib not found!
    echo Please build the main library first:
    echo   cd ..
    echo   msbuild VolkDMA.vcxproj /p:Configuration=Release /p:Platform=x64
    pause
    exit /b 1
)

echo [INFO] VolkDMA.lib found!
echo.

REM Create output directory
if not exist "x64\Release" mkdir x64\Release

echo [INFO] Compiling Real Hardware Test Program...
echo.

cl /EHsc /std:c++20 /O2 /MD /DNOMINMAX ^
   /I"..\include" ^
   /I"..\" ^
   /I"..\external\vmm" ^
   /I"..\external\leechcore" ^
   /I"..\external\VolkLog\include" ^
   /Fe:x64\Release\test_dma.exe ^
   test_dma.cpp ^
   /link ^
   /LIBPATH:"..\x64\Release" VolkDMA.lib ^
   /LIBPATH:"..\external\vmm" vmm.lib ^
   /LIBPATH:"..\external\leechcore" leechcore.lib

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Compilation failed!
    pause
    exit /b 1
)

echo.
echo ====================================================================
echo [SUCCESS] Build complete!
echo ====================================================================
echo.
echo Executable: x64\Release\test_dma.exe
echo.

REM Copy required DLLs
echo [INFO] Copying required DLLs...
if exist "..\external\vmm\vmm.dll" (
    copy "..\external\vmm\vmm.dll" "x64\Release\" >nul 2>&1
    echo   - Copied vmm.dll
)
if exist "..\external\leechcore\leechcore.dll" (
    copy "..\external\leechcore\leechcore.dll" "x64\Release\" >nul 2>&1
    echo   - Copied leechcore.dll
)
if exist "..\dlls\vmm.dll" (
    copy "..\dlls\vmm.dll" "x64\Release\" >nul 2>&1
    echo   - Copied vmm.dll from dlls/
)
if exist "..\dlls\leechcore.dll" (
    copy "..\dlls\leechcore.dll" "x64\Release\" >nul 2>&1
    echo   - Copied leechcore.dll from dlls/
)

echo.
echo ====================================================================
echo IMPORTANT - READ BEFORE RUNNING:
echo ====================================================================
echo.
echo This program uses REAL FPGA DMA hardware!
echo.
echo Requirements:
echo   1. Run as Administrator (RIGHT-CLICK → Run as administrator)
echo   2. FPGA device connected via PCIe
echo   3. Drivers installed (vmm, leechcore)
echo   4. Target PC accessible via DMA
echo.
echo To run:
echo   cd x64\Release
echo   Right-click test_dma.exe → Run as administrator
echo.
echo ====================================================================

pause
