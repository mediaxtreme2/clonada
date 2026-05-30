@echo off
setlocal enabledelayedexpansion

echo.
echo ================================================================
echo.
echo   CLONADA AI VOCAL SUITE - AI Engine Setup
echo   Version 1.2.0
echo   by mediaXtreme LLC
echo.
echo   This will download and configure the AI voice engine.
echo   Internet connection required. Takes 5-15 minutes.
echo.
echo ================================================================
echo.

:: Determine install directory (passed by Inno Setup or default)
if not "%~1"=="" (
    set "APP_DIR=%~1"
) else (
    set "APP_DIR=%~dp0"
)

:: Remove trailing backslash if present
if "%APP_DIR:~-1%"=="\" set "APP_DIR=%APP_DIR:~0,-1%"

set "CLONADA_HOME=%USERPROFILE%\Clonada"
set "CONDA_DIR=%CLONADA_HOME%\miniconda"
set "ENV_NAME=clonada"
set "PYTHON_VER=3.11"

echo [1/7] Setting up Clonada home directory...
if not exist "%CLONADA_HOME%" mkdir "%CLONADA_HOME%"
if not exist "%CLONADA_HOME%\models" mkdir "%CLONADA_HOME%\models"
if not exist "%CLONADA_HOME%\weights" mkdir "%CLONADA_HOME%\weights"
if not exist "%CLONADA_HOME%\presets" mkdir "%CLONADA_HOME%\presets"

:: Copy engine files from install dir to Clonada home
if exist "%APP_DIR%\engine" (
    echo   Copying AI engine files...
    if not exist "%CLONADA_HOME%\python" mkdir "%CLONADA_HOME%\python"
    if not exist "%CLONADA_HOME%\python\lib" mkdir "%CLONADA_HOME%\python\lib"
    xcopy /Y /Q "%APP_DIR%\engine\*.py" "%CLONADA_HOME%\python\" >nul 2>&1
    xcopy /Y /Q "%APP_DIR%\engine\requirements.txt" "%CLONADA_HOME%\python\" >nul 2>&1
    xcopy /Y /Q "%APP_DIR%\engine\lib\*.py" "%CLONADA_HOME%\python\lib\" >nul 2>&1
    echo   [OK] Engine files copied
)
echo   [OK] Home directory: %CLONADA_HOME%

:: ─────────────────────────────────────────────────────
:: STEP 2: Install Miniconda
:: ─────────────────────────────────────────────────────
echo.
if exist "%CONDA_DIR%\condabin\conda.bat" (
    echo [2/7] Miniconda already installed - skipping download
    goto :setup_env
)

echo [2/7] Downloading Miniconda (~80 MB)...
curl -L -o "%CLONADA_HOME%\miniconda_installer.exe" https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe
if errorlevel 1 (
    echo   [ERROR] Failed to download Miniconda. Check internet connection.
    pause
    exit /b 1
)

echo.
echo [3/7] Installing Miniconda (silent, please wait)...
start /wait "%CLONADA_HOME%\miniconda_installer.exe" /S /D=%CONDA_DIR%
del "%CLONADA_HOME%\miniconda_installer.exe" >nul 2>&1
echo   [OK] Miniconda installed

:: ─────────────────────────────────────────────────────
:: STEP 4: Create Python environment
:: ─────────────────────────────────────────────────────
:setup_env
echo.
echo [4/7] Creating Python %PYTHON_VER% environment...

set "ENV_DIR=%CONDA_DIR%\envs\%ENV_NAME%"
if exist "%ENV_DIR%\python.exe" (
    echo   [OK] Environment already exists - skipping
) else (
    call "%CONDA_DIR%\condabin\conda.bat" create -p "%ENV_DIR%" python=%PYTHON_VER% -y -q >nul 2>&1
    if errorlevel 1 (
        echo   [ERROR] Failed to create Python environment
        pause
        exit /b 1
    )
    echo   [OK] Python %PYTHON_VER% environment created
)

:: ─────────────────────────────────────────────────────
:: STEP 5: Install PyTorch
:: ─────────────────────────────────────────────────────
echo.
echo [5/7] Installing PyTorch (AI inference engine)...

:: Check for NVIDIA GPU
set "HAS_NVIDIA=0"
where nvidia-smi >nul 2>&1
if not errorlevel 1 (
    set "HAS_NVIDIA=1"
)

if "%HAS_NVIDIA%"=="1" (
    echo   NVIDIA GPU detected - installing CUDA PyTorch...
    "%ENV_DIR%\python.exe" -m pip install --quiet torch torchaudio --index-url https://download.pytorch.org/whl/cu121
    echo   [OK] PyTorch installed with CUDA GPU acceleration
) else (
    echo   No NVIDIA GPU detected - installing CPU PyTorch...
    "%ENV_DIR%\python.exe" -m pip install --quiet torch torchaudio --index-url https://download.pytorch.org/whl/cpu
    echo   [OK] PyTorch installed (CPU mode)
    echo   TIP: If you have an NVIDIA GPU, reinstall with CUDA for 10x faster processing:
    echo        %ENV_DIR%\python.exe -m pip install torch torchaudio --index-url https://download.pytorch.org/whl/cu121
)

:: ─────────────────────────────────────────────────────
:: STEP 6: Install AI dependencies
:: ─────────────────────────────────────────────────────
echo.
echo [6/7] Installing AI dependencies (Demucs, RVC, etc.)...
echo   This may take a few minutes...

if exist "%CLONADA_HOME%\python\requirements.txt" (
    "%ENV_DIR%\python.exe" -m pip install --quiet -r "%CLONADA_HOME%\python\requirements.txt"
    if errorlevel 1 (
        echo   [WARNING] Some dependencies failed to install. The engine may still work.
    ) else (
        echo   [OK] All AI dependencies installed
    )
) else (
    echo   [WARNING] requirements.txt not found - skipping
)

:: ─────────────────────────────────────────────────────
:: STEP 7: Create launcher scripts + shortcuts
:: ─────────────────────────────────────────────────────
echo.
echo [7/7] Creating launcher scripts and shortcuts...

:: Engine launcher
(
echo @echo off
echo echo Starting Clonada AI Engine...
echo echo Press Ctrl+C to stop
echo echo.
echo call "%CONDA_DIR%\condabin\conda.bat" activate "%ENV_DIR%"
echo python "%CLONADA_HOME%\python\clonada_server.py" --models-dir "%CLONADA_HOME%\models" --weights-dir "%CLONADA_HOME%\weights" %%*
) > "%CLONADA_HOME%\start_engine.bat"
echo   [OK] start_engine.bat created

:: License activation
(
echo @echo off
echo echo Clonada License Activation
echo echo ==========================
echo echo.
echo call "%CONDA_DIR%\condabin\conda.bat" activate "%ENV_DIR%"
echo set /p KEY="Enter license key: "
echo python "%CLONADA_HOME%\python\clonada_server.py" --license-key %%KEY%%
echo pause
) > "%CLONADA_HOME%\activate_license.bat"
echo   [OK] activate_license.bat created

:: Uninstaller
(
echo @echo off
echo echo Clonada Uninstaller
echo echo ===================
echo echo.
echo set /p CONFIRM="Remove Clonada and all data? (Y/N): "
echo if /i not "%%CONFIRM%%"=="Y" exit /b
echo echo Removing plugins...
echo rd /s /q "%CommonProgramFiles%\VST3\Clonada.vst3" 2^>nul
echo rd /s /q "%CommonProgramFiles%\CLAP\Clonada.clap" 2^>nul
echo echo Removing Clonada home...
echo rd /s /q "%CLONADA_HOME%" 2^>nul
echo echo Clonada has been completely removed.
echo pause
) > "%CLONADA_HOME%\uninstall.bat"
echo   [OK] uninstall.bat created

:: Desktop shortcut for engine
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut([Environment]::GetFolderPath('Desktop') + '\Clonada Engine.lnk'); $Shortcut.TargetPath = '%CLONADA_HOME%\start_engine.bat'; $Shortcut.WorkingDirectory = '%CLONADA_HOME%'; $Shortcut.Description = 'Clonada AI Voice Engine'; $Shortcut.Save()" >nul 2>&1
echo   [OK] Desktop shortcut created

:: Save install path for the plugin to find the engine
reg add "HKCU\SOFTWARE\mediaXtreme\Clonada" /v "EngineDir" /t REG_SZ /d "%CLONADA_HOME%" /f >nul 2>&1
reg add "HKCU\SOFTWARE\mediaXtreme\Clonada" /v "CondaEnv" /t REG_SZ /d "%ENV_DIR%" /f >nul 2>&1
echo   [OK] Registry paths set

echo.
echo ================================================================
echo.
echo   INSTALLATION COMPLETE!
echo.
echo   Plugin locations:
echo     VST3  : %CommonProgramFiles%\VST3\Clonada.vst3
echo     CLAP  : %CommonProgramFiles%\CLAP\
echo     App   : %APP_DIR%\Clonada.exe
echo.
echo   AI Engine:
echo     Home  : %CLONADA_HOME%
echo     Start : %CLONADA_HOME%\start_engine.bat
echo.
echo   Next steps:
echo     1. Open your DAW and rescan plugins
echo     2. Load Clonada on a vocal track
echo     3. Enter your license key in the plugin
echo     4. AI models download automatically on first use (~200 MB)
echo.
echo   To uninstall: %CLONADA_HOME%\uninstall.bat
echo.
echo ================================================================
echo.
pause
