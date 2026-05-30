#!/bin/bash
set -e

echo "============================================================"
echo "  CLONADA AI VOCAL SUITE - macOS Installer"
echo "  Version 1.0.0"
echo "============================================================"
echo ""

INSTALL_DIR="$HOME/Clonada"
CONDA_DIR="$INSTALL_DIR/miniconda"
ENV_NAME="clonada"
PYTHON_VER="3.11"

echo "[1/6] Creating installation directory..."
mkdir -p "$INSTALL_DIR"
cd "$INSTALL_DIR"

# Check if Miniconda already installed
if [ -f "$CONDA_DIR/bin/conda" ]; then
    echo "[OK] Miniconda already installed"
else
    echo "[2/6] Downloading Miniconda..."
    ARCH=$(uname -m)
    if [ "$ARCH" = "arm64" ]; then
        CONDA_URL="https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-arm64.sh"
    else
        CONDA_URL="https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-x86_64.sh"
    fi

    curl -L -o miniconda_installer.sh "$CONDA_URL"
    chmod +x miniconda_installer.sh

    echo "[3/6] Installing Miniconda (silent)..."
    bash miniconda_installer.sh -b -p "$CONDA_DIR"
    rm miniconda_installer.sh
fi

ENV_DIR="$CONDA_DIR/envs/$ENV_NAME"

echo "[4/6] Creating Python environment..."
if [ -d "$ENV_DIR" ]; then
    echo "[OK] Environment already exists"
else
    "$CONDA_DIR/bin/conda" create -p "$ENV_DIR" python="$PYTHON_VER" -y
fi

echo "[5/6] Installing dependencies..."
export PATH="$ENV_DIR/bin:$CONDA_DIR/bin:$PATH"

# Install PyTorch (MPS for Apple Silicon, CPU for Intel)
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    "$ENV_DIR/bin/pip" install torch torchaudio
    echo "[OK] PyTorch installed with MPS (Apple Silicon) support"
else
    "$ENV_DIR/bin/pip" install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
    echo "[OK] PyTorch installed (CPU)"
fi

"$ENV_DIR/bin/pip" install -r "$INSTALL_DIR/python/requirements.txt"

echo "[6/6] Setting up..."
mkdir -p "$INSTALL_DIR/weights" "$INSTALL_DIR/models"

# Create launcher script
cat > "$INSTALL_DIR/start_engine.sh" << 'LAUNCHER'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_DIR="$SCRIPT_DIR/miniconda/envs/clonada"
export PATH="$ENV_DIR/bin:$SCRIPT_DIR/miniconda/bin:$PATH"
"$ENV_DIR/bin/python" "$SCRIPT_DIR/python/clonada_server.py" \
    --models-dir "$SCRIPT_DIR/models" \
    --weights-dir "$SCRIPT_DIR/weights" "$@"
LAUNCHER
chmod +x "$INSTALL_DIR/start_engine.sh"

# Create license activation script
cat > "$INSTALL_DIR/activate_license.sh" << 'ACTIVATE'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_DIR="$SCRIPT_DIR/miniconda/envs/clonada"
export PATH="$ENV_DIR/bin:$SCRIPT_DIR/miniconda/bin:$PATH"
read -p "Enter license key: " KEY
"$ENV_DIR/bin/python" "$SCRIPT_DIR/python/clonada_server.py" --license-key "$KEY"
ACTIVATE
chmod +x "$INSTALL_DIR/activate_license.sh"

echo ""
echo "============================================================"
echo "  INSTALLATION COMPLETE!"
echo ""
echo "  Install dir: $INSTALL_DIR"
echo "  Start engine: ./start_engine.sh"
echo "  Activate license: ./activate_license.sh"
echo "============================================================"
echo ""
