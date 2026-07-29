@echo off
setlocal enabledelayedexpansion
:: Package Easy SSH into a single NSIS installer file via CPack.
:: Usage: package-windows.bat <target-triple> <binary-dir> [out-dir]

if "%~1"=="" (
    echo usage: package-windows.bat ^<target-triple^> ^<binary-dir^> [out-dir] >&2
    exit /b 1
)
if "%~2"=="" (
    echo usage: package-windows.bat ^<target-triple^> ^<binary-dir^> [out-dir] >&2
    exit /b 1
)

set "TARGET=%~1"
set "BINARY_DIR=%~f2"
set "OUT_DIR=%~f3"
if "%OUT_DIR%"=="" set "OUT_DIR=%CD%\dist"

mkdir "%OUT_DIR%" 2>nul

:: -------------------------------------------------------------------
:: windeployqt prep: stage qt6keychain.dll into Qt bin dir
:: -------------------------------------------------------------------
call :find_qt_bin QT_BIN
if defined QT_BIN (
    call :find_keychain_dll KEYCHAIN_DLL
    if defined KEYCHAIN_DLL (
        if not exist "%QT_BIN%\qt6keychain.dll" (
            copy /y "!KEYCHAIN_DLL!" "%QT_BIN%\qt6keychain.dll" >nul
            echo package: Staged qt6keychain.dll into %QT_BIN% >&2
        )
    )
)

:: -------------------------------------------------------------------
:: Build installer with CPack NSIS
:: -------------------------------------------------------------------
echo package: Running CPack NSIS from %BINARY_DIR% >&2
pushd "%BINARY_DIR%"
cpack -C Release -G NSIS
if errorlevel 1 (
    popd
    echo error: cpack NSIS generation failed >&2
    exit /b 1
)

set "NSIS_OUT="
for %%F in (easy-ssh-*.exe *-win64.exe *.exe) do (
    if exist "%%~fF" if /I not "%%~nxF"=="easy-ssh.exe" (
        set "NSIS_OUT=%%~fF"
        goto :found_installer
    )
)

:found_installer
popd

if "%NSIS_OUT%"=="" (
    echo error: NSIS installer not found in %BINARY_DIR% >&2
    dir "%BINARY_DIR%" >&2
    exit /b 1
)

set "DEST=%OUT_DIR%\easy-ssh-%TARGET%-setup.exe"
copy /y "%NSIS_OUT%" "%DEST%" >nul
if errorlevel 1 (
    echo error: failed to copy installer to %DEST% >&2
    exit /b 1
)

echo package: Created %DEST% >&2
for %%A in ("%DEST%") do echo package: Size %%~zA bytes >&2
exit /b 0

:: ===================================================================
:: Subroutines
:: ===================================================================

:find_qt_bin
if defined QT_ROOT_DIR (
    if exist "%QT_ROOT_DIR%\bin" (
        set "%~1=%QT_ROOT_DIR%\bin"
        exit /b
    )
)
where windeployqt >nul 2>&1 && for /f "delims=" %%P in ('where windeployqt') do (
    set "%~1=%%~dpP"
    exit /b
)
where windeployqt6 >nul 2>&1 && for /f "delims=" %%P in ('where windeployqt6') do (
    set "%~1=%%~dpP"
    exit /b
)
exit /b

:find_keychain_dll
if defined PREFIX (
    if exist "%PREFIX%\bin\qt6keychain.dll" (
        set "%~1=%PREFIX%\bin\qt6keychain.dll"
        exit /b
    )
)
if defined CMAKE_PREFIX_PATH (
    for %%P in ("%CMAKE_PREFIX_PATH:;=";"%") do (
        if exist "%%~P\bin\qt6keychain.dll" (
            set "%~1=%%~P\bin\qt6keychain.dll"
            exit /b
        )
    )
)
exit /b
