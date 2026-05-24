@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo   CLONADA AI VOCAL SUITE - Windows Installer
echo   Version 1.0.0
echo ============================================================
echo.

set "INSTALL_DIR=%USERPROFILE%\Clonada"
set "CONDA_DIR=%INSTALL_DIR%\miniconda"
set "ENV_NAME=clonada"
set "PYTHON_VER=3.11"

echo [1/6] Creating installation directory...
if not exist "%INSTALL_DIR%" mkdir "%INSTALL_DIR%"
cd /d "%INSTALL_DIR%"

:: Check if Miniconda already installed
if exist "%CONDA_DIR%\condabin\conda.bat" (
    echo [OK] Miniconda already installed
    goto :setup_env
)

echo [2/6] Downloading Miniconda...
curl -L -o miniconda_installer.exe https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe
if errorlevel 1 (
    echo [ERROR] Failed to download Miniconda
    pause
    exit /b 1
)

echo [3/6] Installing Miniconda (silent)...
start /wait miniconda_installer.exe /S /D=%CONDA_DIR%
del miniconda_installer.exe

:setup_env
echo [4/6] Creating Python environment...
call "%CONDA_DIR%\condabin\conda.bat" create -n %ENV_NAME% python=%PYTHON_VER% -y 2>nul

echo [5/6] Installing dependencies...
call "%CONDA_DIR%\condabin\conda.bat" activate %ENV_NAME%

:: Install PyTorch (CPU by default, user can install CUDA version later)
pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip install -r "%INSTALL_DIR%\python\requirements.txt"

echo [6/6] Downloading base models...
if not exist "%INSTALL_DIR%\weights" mkdir "%INSTALL_DIR%\weights"

:: Download HuBERT (will use transformers auto-download on first run instead)
echo Base models will be downloaded on first run.

:: Create launcher scripts
echo @echo off > "%INSTALL_DIR%\start_engine.bat"
echo call "%CONDA_DIR%\condabin\conda.bat" activate %ENV_NAME% >> "%INSTALL_DIR%\start_engine.bat"
echo python "%INSTALL_DIR%\python\clonada_server.py" --models-dir "%INSTALL_DIR%\models" --weights-dir "%INSTALL_DIR%\weights" %%* >> "%INSTALL_DIR%\start_engine.bat"

echo @echo off > "%INSTALL_DIR%\activate_license.bat"
echo call "%CONDA_DIR%\condabin\conda.bat" activate %ENV_NAME% >> "%INSTALL_DIR%\activate_license.bat"
echo set /p KEY="Enter license key: " >> "%INSTALL_DIR%\activate_license.bat"
echo python "%INSTALL_DIR%\python\clonada_server.py" --license-key %%KEY%% >> "%INSTALL_DIR%\activate_license.bat"

:: Create desktop shortcut
echo Creating desktop shortcut...
set "SHORTCUT=%USERPROFILE%\Desktop\Clonada Engine.lnk"
powershell -Command "$WshShell = New-Object -ComObject WScript.Shell; $Shortcut = $WshShell.CreateShortcut('%SHORTCUT%'); $Shortcut.TargetPath = '%INSTALL_DIR%\start_engine.bat'; $Shortcut.WorkingDirectory = '%INSTALL_DIR%'; $Shortcut.Description = 'Clonada AI Vocal Suite'; $Shortcut.Save()"

echo.
echo ============================================================
echo   INSTALLATION COMPLETE!
echo.
echo   Install dir: %INSTALL_DIR%
echo   Start engine: start_engine.bat
echo   Activate license: activate_license.bat
echo.
echo   For GPU acceleration, install CUDA PyTorch:
echo   pip install torch torchaudio --index-url https://download.pytorch.org/whl/cu121
echo ============================================================
echo.
pause
