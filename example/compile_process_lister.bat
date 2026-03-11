@echo off
echo Compiling Process Lister...
echo.

cl.exe /nologo /EHsc /std:c++20 /MT /I"..\include" /I".." /I"..\external\vmm" /I"..\external\leechcore" ^
    list_processes.cpp ^
    ..\external\vmm\vmm.lib ^
    ..\external\leechcore\leechcore.lib ^
    /Fe:x64\Release\list_processes.exe ^
    /Fo:x64\Release\

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Compiled: x64\Release\list_processes.exe
    echo.
    echo Run it to see all processes on your target PC!
) else (
    echo.
    echo [ERROR] Compilation failed!
)

pause
