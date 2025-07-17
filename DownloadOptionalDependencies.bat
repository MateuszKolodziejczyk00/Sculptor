@echo off

set "REPO_URL=https://github.com/MateuszKolodziejczyk00/SHARC"
set "CLONE_DIR=%~dp0Saved/Temp/Sharc"
set "SRC_SUBPATH=include"
set "DEST_DIR=%~dp0Shaders/Sharc"
set "POST_CLONE_WAIT=0"

if exist "%CLONE_DIR%" (
    echo Removing existing clone dir "%CLONE_DIR%"...
    rmdir /s /q "%CLONE_DIR%"
)

git clone "%REPO_URL%" "%CLONE_DIR%"
if errorlevel 1 (
    echo [ERROR] git clone failed
    goto :end_fail
)

set "FULL_SRC=%CLONE_DIR%\%SRC_SUBPATH%"
if not exist "%FULL_SRC%" (
    echo [ERROR] Source path "%FULL_SRC%" not found in cloned repo
    goto :end_fail
)

if not exist "%DEST_DIR%" (
    mkdir "%DEST_DIR%" 2>nul
)

robocopy "%FULL_SRC%" "%DEST_DIR%" /MIR
rmdir /s /q "%CLONE_DIR%"

:end_fail
pause