@echo off
setlocal enabledelayedexpansion
:: Package Easy SSH into a single portable exe using Enigma Virtual Box.
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

set "STAGE=%OUT_DIR%\stage"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"

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
:: Install (runs Qt deploy script / windeployqt)
:: -------------------------------------------------------------------
echo package: Installing into %STAGE% >&2
cmake --install "%BINARY_DIR%" --prefix "%STAGE%"
if errorlevel 1 exit /b 1

:: -------------------------------------------------------------------
:: Find the exe
:: -------------------------------------------------------------------
set "EXE="
if exist "%STAGE%\bin\easy-ssh.exe" set "EXE=%STAGE%\bin\easy-ssh.exe"
if "%EXE%"=="" if exist "%STAGE%\easy-ssh.exe" set "EXE=%STAGE%\easy-ssh.exe"
if "%EXE%"=="" (
    echo error: easy-ssh.exe not found under %STAGE% >&2
    exit /b 1
)
for %%F in ("%EXE%") do set "EXE_DIR=%%~dpF"
:: Remove trailing backslash
if "%EXE_DIR:~-1%"=="\" set "EXE_DIR=%EXE_DIR:~0,-1%"

:: -------------------------------------------------------------------
:: Run windeployqt on the staged exe
:: -------------------------------------------------------------------
where windeployqt >nul 2>&1 && (
    echo package: Running windeployqt >&2
    windeployqt --release --no-translations "%EXE%" 2>nul || windeployqt "%EXE%" 2>nul || echo package: windeployqt warning >&2
)

:: -------------------------------------------------------------------
:: Copy third-party DLLs not handled by windeployqt
:: -------------------------------------------------------------------
call :copy_extra_dlls

:: -------------------------------------------------------------------
:: Enigma Virtual Box
:: -------------------------------------------------------------------
set "EVB_CONSOLE="
if defined ENIGMA_VB_DIR (
    if exist "%ENIGMA_VB_DIR%\enigmavbconsole.exe" set "EVB_CONSOLE=%ENIGMA_VB_DIR%\enigmavbconsole.exe"
)
if "%EVB_CONSOLE%"=="" (
    if exist "C:\Program Files\Enigma Virtual Box\enigmavbconsole.exe" (
        set "EVB_CONSOLE=C:\Program Files\Enigma Virtual Box\enigmavbconsole.exe"
    )
)
if "%EVB_CONSOLE%"=="" (
    where enigmavbconsole.exe >nul 2>&1 && for /f "delims=" %%P in ('where enigmavbconsole.exe') do set "EVB_CONSOLE=%%P"
)
if "%EVB_CONSOLE%"=="" (
    echo error: enigmavbconsole.exe not found ^(set ENIGMA_VB_DIR^) >&2
    exit /b 1
)

set "PORTABLE=easy-ssh-%TARGET%.exe"
set "EVB_PROJECT=%OUT_DIR%\easy-ssh.evb"
set "OUTPUT_EXE=%OUT_DIR%\%PORTABLE%"

echo package: Generating Enigma Virtual Box project >&2

:: Build the .evb XML project file
> "%EVB_PROJECT%" echo ^<EVBProject^>
>> "%EVB_PROJECT%" echo   ^<InputFile^>%EXE%^</InputFile^>
>> "%EVB_PROJECT%" echo   ^<OutputFile^>%OUTPUT_EXE%^</OutputFile^>
>> "%EVB_PROJECT%" echo   ^<Files^>
>> "%EVB_PROJECT%" echo     ^<Enabled^>true^</Enabled^>
>> "%EVB_PROJECT%" echo     ^<DeleteExtracted^>true^</DeleteExtracted^>
>> "%EVB_PROJECT%" echo     ^<CompressFiles^>true^</CompressFiles^>

:: Add DLLs in exe directory
for %%D in ("%EXE_DIR%\*.dll") do (
    >> "%EVB_PROJECT%" echo     ^<File^>
    >> "%EVB_PROJECT%" echo       ^<Type^>3^</Type^>
    >> "%EVB_PROJECT%" echo       ^<Name^>%%DEFAULT FOLDER%%\%%~nxD^</Name^>
    >> "%EVB_PROJECT%" echo       ^<File^>%%~fD^</File^>
    >> "%EVB_PROJECT%" echo       ^<Options^>0^</Options^>
    >> "%EVB_PROJECT%" echo     ^</File^>
)

:: Add subdirectories (platforms, styles, imageformats, etc.)
for /d %%S in ("%EXE_DIR%\*") do (
    >> "%EVB_PROJECT%" echo     ^<File^>
    >> "%EVB_PROJECT%" echo       ^<Type^>2^</Type^>
    >> "%EVB_PROJECT%" echo       ^<Name^>%%DEFAULT FOLDER%%\%%~nxS^</Name^>
    >> "%EVB_PROJECT%" echo       ^<Options^>0^</Options^>
    for %%F in ("%%S\*") do (
        >> "%EVB_PROJECT%" echo       ^<File^>
        >> "%EVB_PROJECT%" echo         ^<Type^>3^</Type^>
        >> "%EVB_PROJECT%" echo         ^<Name^>%%DEFAULT FOLDER%%\%%~nxS\%%~nxF^</Name^>
        >> "%EVB_PROJECT%" echo         ^<File^>%%~fF^</File^>
        >> "%EVB_PROJECT%" echo         ^<Options^>0^</Options^>
        >> "%EVB_PROJECT%" echo       ^</File^>
    )
    >> "%EVB_PROJECT%" echo     ^</File^>
)

>> "%EVB_PROJECT%" echo   ^</Files^>
>> "%EVB_PROJECT%" echo   ^<Registries^>
>> "%EVB_PROJECT%" echo     ^<Enabled^>false^</Enabled^>
>> "%EVB_PROJECT%" echo   ^</Registries^>
>> "%EVB_PROJECT%" echo ^</EVBProject^>

echo package: Running Enigma Virtual Box console packer >&2
"%EVB_CONSOLE%" "%EVB_PROJECT%"
if errorlevel 1 (
    echo error: Enigma Virtual Box failed >&2
    exit /b 1
)

if not exist "%OUTPUT_EXE%" (
    echo error: Enigma Virtual Box did not produce %OUTPUT_EXE% >&2
    exit /b 1
)

del /q "%EVB_PROJECT%" 2>nul
echo package: Created %OUTPUT_EXE% >&2
for %%A in ("%OUTPUT_EXE%") do echo   %%~zA bytes >&2

:: Cleanup
rmdir /s /q "%STAGE%" 2>nul
exit /b 0

:: ===================================================================
:: Subroutines
:: ===================================================================

:find_qt_bin
:: Sets %1 to the Qt bin directory path
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
:: Sets %1 to path of qt6keychain.dll if found in PREFIX or CMAKE_PREFIX_PATH
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

:copy_extra_dlls
:: Copy third-party DLLs that windeployqt doesn't handle
set "DLL_NAMES=ssh.dll libssh.dll qtermwidget6.dll qt6keychain.dll qtkeychain.dll libcrypto-3-x64.dll libssl-3-x64.dll libcrypto-1_1-x64.dll libssl-1_1-x64.dll zlib1.dll zlib.dll"

:: Search in PREFIX
if defined PREFIX call :search_dlls_in "%PREFIX%"
:: Search in VCPKG_ROOT
if defined VCPKG_ROOT call :search_dlls_in "%VCPKG_ROOT%\installed\x64-windows"
:: Search in CMAKE_PREFIX_PATH
if defined CMAKE_PREFIX_PATH (
    for %%P in ("%CMAKE_PREFIX_PATH:;=";"%") do (
        call :search_dlls_in "%%~P"
    )
)
exit /b

:search_dlls_in
:: %1 = root directory to search
if not exist "%~1" exit /b
for %%N in (%DLL_NAMES%) do (
    if not exist "%EXE_DIR%\%%N" (
        for /r "%~1" %%F in (%%N) do (
            if exist "%%F" (
                copy /y "%%F" "%EXE_DIR%\%%N" >nul 2>&1
                echo package:   + %%N >&2
            )
        )
    )
)
exit /b
