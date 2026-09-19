@echo off
setlocal enabledelayedexpansion

:: --- CONFIGURATION ---
set "URL=https://github.com/shader-slang/slang/releases/download/v2026.18/slang-2026.18-windows-x86_64.zip"
set "TARGET_DIR=Source\ThirdParty\Slang\Slang"
set "ZIP_FILE=slang_temp.zip"

echo ============================================
echo [Slang Setup] Starting download process...
echo ============================================

:: 1. Create the target directory if it doesn't exist
if not exist "%TARGET_DIR%" (
    echo [Slang Setup] Creating directory: %TARGET_DIR%
    mkdir "%TARGET_DIR%"
)

:: 2. Download the zip file using curl (native in modern Windows)
echo [Slang Setup] Downloading Slang archive...
curl -L -f -o "%ZIP_FILE%" "%URL%"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Download failed. Please check the URL or your internet connection.
    pause
    exit /b 1
)

:: 3. Extract the contents using tar (native in Windows 10/11)
echo [Slang Setup] Extracting files into %TARGET_DIR%...
tar -xf "%ZIP_FILE%" -C "%TARGET_DIR%"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Extraction failed.
    pause
    exit /b 1
)

:: 4. Clean up the zip archive
echo [Slang Setup] Cleaning up temporary archive...
del "%ZIP_FILE%"

echo ============================================
echo [Slang Setup] Success! Slang is ready in %TARGET_DIR%
echo ============================================
endlocal
pause

