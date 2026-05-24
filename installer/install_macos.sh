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

echo "[4/6] Creating Python environment..."
"$CONDA_DIR/bin/conda" create -n "$ENV_NAME" python="$PYTHON_VER" -y 2>/dev/null || true

echo "[5/6] Installing dependencies..."
source "$CONDA_DIR/bin/activate" "$ENV_NAME"

# Install PyTorch (MPS for Apple Silicon, CPU for Intel)
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    pip install torch torchaudio
    echo "[OK] PyTorch installed with MPS (Apple Silicon) support"
else
    pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
    echo "[OK] PyTorch installed (CPU)"
fi

pip install -r "$INSTALL_DIR/python/requirements.txt"

echo "[6/6] Setting up..."
mkdir -p "$INSTALL_DIR/weights" "$INSTALL_DIR/models"

# Create launcher script
cat > "$INSTALL_DIR/start_engine.sh" << 'LAUNCHER'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/miniconda/bin/activate" clonada
python "$SCRIPT_DIR/python/clonada_server.py" \
    --models-dir "$SCRIPT_DIR/models" \
    --weights-dir "$SCRIPT_DIR/weights" "$@"
LAUNCHER
chmod +x "$INSTALL_DIR/start_engine.sh"

# Create license activation script
cat > "$INSTALL_DIR/activate_license.sh" << 'ACTIVATE'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/miniconda/bin/activate" clonada
read -p "Enter license key: " KEY
python "$SCRIPT_DIR/python/clonada_server.py" --license-key "$KEY"
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
