#!/bin/bash
set -e

# ════════════════════════════════════════════════════════════════
#   CLONADA AI VOCAL SUITE — Unified macOS Installer
#   Installs plugin formats + Python AI engine in one step
# ════════════════════════════════════════════════════════════════

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="1.6.6"
INSTALL_DIR="$HOME/Clonada"
CONDA_DIR="$INSTALL_DIR/miniconda"
ENV_NAME="clonada"
PYTHON_VER="3.11"
TOTAL_STEPS=8
CURRENT=0

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

step() {
    CURRENT=$((CURRENT + 1))
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD}  [${CURRENT}/${TOTAL_STEPS}] $1${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

ok() {
    echo -e "  ${GREEN}✓${NC} $1"
}

warn() {
    echo -e "  ${YELLOW}!${NC} $1"
}

fail() {
    echo -e "  ${RED}✗ ERROR:${NC} $1"
    exit 1
}

echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║                                                      ║${NC}"
echo -e "${BOLD}║     CLONADA AI VOCAL SUITE — macOS Installer         ║${NC}"
echo -e "${BOLD}║     Version ${VERSION}                                    ║${NC}"
echo -e "${BOLD}║     by mediaXtreme LLC                               ║${NC}"
echo -e "${BOLD}║                                                      ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  This installer will set up everything you need:"
echo -e "    • Audio plugins (VST3, AU, CLAP)"
echo -e "    • Standalone application"
echo -e "    • AI voice engine (Python + PyTorch)"
echo ""
echo -e "  ${YELLOW}The AI engine download may take 5-15 minutes${NC}"
echo -e "  ${YELLOW}depending on your internet connection.${NC}"
echo ""

# ──────────────────────────────────────────────────────
# STEP 1: Install Audio Plugins
# ──────────────────────────────────────────────────────
step "Installing audio plugins"

PLUGINS_INSTALLED=0

# VST3
if [ -d "$SCRIPT_DIR/Clonada.vst3" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$DEST"
    rm -rf "$DEST/Clonada.vst3"
    cp -R "$SCRIPT_DIR/Clonada.vst3" "$DEST/"
    ok "VST3 → ~/Library/Audio/Plug-Ins/VST3/"
    PLUGINS_INSTALLED=$((PLUGINS_INSTALLED + 1))
fi

# Audio Unit
if [ -d "$SCRIPT_DIR/Clonada.component" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$DEST"
    rm -rf "$DEST/Clonada.component"
    cp -R "$SCRIPT_DIR/Clonada.component" "$DEST/"
    ok "Audio Unit → ~/Library/Audio/Plug-Ins/Components/"
    PLUGINS_INSTALLED=$((PLUGINS_INSTALLED + 1))
fi

# CLAP
if [ -f "$SCRIPT_DIR/Clonada.clap" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/CLAP"
    mkdir -p "$DEST"
    rm -f "$DEST/Clonada.clap"
    cp "$SCRIPT_DIR/Clonada.clap" "$DEST/"
    ok "CLAP → ~/Library/Audio/Plug-Ins/CLAP/"
    PLUGINS_INSTALLED=$((PLUGINS_INSTALLED + 1))
fi

if [ $PLUGINS_INSTALLED -eq 0 ]; then
    warn "No plugin binaries found in DMG — skipping plugin install"
fi

# ──────────────────────────────────────────────────────
# STEP 2: Install Standalone App
# ──────────────────────────────────────────────────────
step "Installing standalone application"

if [ -d "$SCRIPT_DIR/Clonada.app" ]; then
    rm -rf /Applications/Clonada.app
    cp -R "$SCRIPT_DIR/Clonada.app" /Applications/
    ok "Clonada.app → /Applications/"
else
    warn "No standalone app found — skipping"
fi

# ──────────────────────────────────────────────────────
# STEP 3: Create Clonada home directory
# ──────────────────────────────────────────────────────
step "Setting up Clonada directory"

mkdir -p "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR/weights"
mkdir -p "$INSTALL_DIR/models"
mkdir -p "$HOME/Library/Application Support/Clonada/Presets"

# Copy Python engine files
if [ -d "$SCRIPT_DIR/python" ]; then
    cp -R "$SCRIPT_DIR/python" "$INSTALL_DIR/"
    ok "Python engine files → ~/Clonada/python/"
elif [ -d "$INSTALL_DIR/python" ]; then
    ok "Python engine files already present"
else
    warn "No Python engine files found"
fi

ok "Created ~/Clonada/ with models, weights, presets directories"

# ──────────────────────────────────────────────────────
# STEP 4: Install Miniconda
# ──────────────────────────────────────────────────────
step "Installing Miniconda (Python package manager)"

if [ -f "$CONDA_DIR/bin/conda" ]; then
    ok "Miniconda already installed — skipping download"
else
    ARCH=$(uname -m)
    if [ "$ARCH" = "arm64" ]; then
        CONDA_URL="https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-arm64.sh"
        ok "Detected Apple Silicon (M-series)"
    else
        CONDA_URL="https://repo.anaconda.com/miniconda/Miniconda3-latest-MacOSX-x86_64.sh"
        ok "Detected Intel Mac"
    fi

    echo "  Downloading Miniconda (~80 MB)..."
    curl -L --progress-bar -o "$INSTALL_DIR/miniconda_installer.sh" "$CONDA_URL" || \
        fail "Failed to download Miniconda. Check your internet connection."

    echo "  Installing Miniconda (silent)..."
    bash "$INSTALL_DIR/miniconda_installer.sh" -b -p "$CONDA_DIR" || \
        fail "Miniconda installation failed"
    rm -f "$INSTALL_DIR/miniconda_installer.sh"
    ok "Miniconda installed to ~/Clonada/miniconda/"
fi

# ──────────────────────────────────────────────────────
# STEP 5: Create Python environment
# ──────────────────────────────────────────────────────
step "Creating Python ${PYTHON_VER} environment"

ENV_DIR="$CONDA_DIR/envs/$ENV_NAME"

if [ -d "$ENV_DIR" ] && [ -f "$ENV_DIR/bin/python" ]; then
    ok "Environment 'clonada' already exists — skipping"
else
    # Use conda-forge to avoid Anaconda ToS channel issues
    "$CONDA_DIR/bin/conda" config --add channels conda-forge 2>/dev/null || true
    "$CONDA_DIR/bin/conda" config --remove channels defaults 2>/dev/null || true
    "$CONDA_DIR/bin/conda" config --set channel_priority strict 2>/dev/null || true
    "$CONDA_DIR/bin/conda" create -p "$ENV_DIR" python="$PYTHON_VER" -c conda-forge --override-channels -y -q || \
        fail "Failed to create Python environment"
    ok "Python ${PYTHON_VER} environment created"
fi

# ──────────────────────────────────────────────────────
# STEP 6: Install PyTorch
# ──────────────────────────────────────────────────────
step "Installing PyTorch (AI inference engine)"

ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
    echo "  Installing PyTorch with Metal Performance Shaders (MPS)..."
    "$ENV_DIR/bin/pip" install --quiet torch torchaudio || \
        fail "PyTorch installation failed"
    ok "PyTorch installed with Apple Silicon GPU acceleration"
else
    echo "  Installing PyTorch (CPU)..."
    "$ENV_DIR/bin/pip" install --quiet torch torchaudio --index-url https://download.pytorch.org/whl/cpu || \
        fail "PyTorch installation failed"
    ok "PyTorch installed (CPU mode)"
fi

# ──────────────────────────────────────────────────────
# STEP 7: Install AI dependencies
# ──────────────────────────────────────────────────────
step "Installing AI dependencies (Demucs, RVC, etc.)"

if [ -f "$INSTALL_DIR/python/requirements.txt" ]; then
    echo "  This may take a few minutes..."
    "$ENV_DIR/bin/pip" install --quiet -r "$INSTALL_DIR/python/requirements.txt" || \
        fail "Dependency installation failed"
    ok "All AI dependencies installed"
else
    warn "requirements.txt not found — skipping dependency install"
fi

# ──────────────────────────────────────────────────────
# STEP 8: Create launcher scripts
# ──────────────────────────────────────────────────────
step "Creating launcher scripts"

# Engine launcher
cat > "$INSTALL_DIR/start_engine.sh" << 'LAUNCHER'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_DIR="$SCRIPT_DIR/miniconda/envs/clonada"
export PATH="$ENV_DIR/bin:$SCRIPT_DIR/miniconda/bin:$PATH"
echo "Starting Clonada AI Engine..."
echo "Press Ctrl+C to stop"
echo ""
"$ENV_DIR/bin/python" "$SCRIPT_DIR/python/clonada_server.py" \
    --models-dir "$SCRIPT_DIR/models" \
    --weights-dir "$SCRIPT_DIR/weights" "$@"
LAUNCHER
chmod +x "$INSTALL_DIR/start_engine.sh"
ok "start_engine.sh created"

# License activation
cat > "$INSTALL_DIR/activate_license.sh" << 'ACTIVATE'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_DIR="$SCRIPT_DIR/miniconda/envs/clonada"
export PATH="$ENV_DIR/bin:$SCRIPT_DIR/miniconda/bin:$PATH"
echo "Clonada License Activation"
echo "=========================="
read -p "Enter license key: " KEY
"$ENV_DIR/bin/python" "$SCRIPT_DIR/python/clonada_server.py" --license-key "$KEY"
ACTIVATE
chmod +x "$INSTALL_DIR/activate_license.sh"
ok "activate_license.sh created"

# Uninstaller
cat > "$INSTALL_DIR/uninstall.sh" << 'UNINSTALL'
#!/bin/bash
echo "Clonada Uninstaller"
echo "==================="
echo ""
read -p "This will remove Clonada and all its data. Continue? (y/N) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Clonada.vst3"
    rm -rf "$HOME/Library/Audio/Plug-Ins/Components/Clonada.component"
    rm -rf "$HOME/Library/Audio/Plug-Ins/CLAP/Clonada.clap"
    rm -rf "/Applications/Clonada.app"
    rm -rf "$HOME/Library/Application Support/Clonada"
    rm -rf "$HOME/Clonada"
    echo "Clonada has been completely removed."
else
    echo "Uninstall cancelled."
fi
UNINSTALL
chmod +x "$INSTALL_DIR/uninstall.sh"
ok "uninstall.sh created"

# ──────────────────────────────────────────────────────
# DONE
# ──────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║                                                      ║${NC}"
echo -e "${GREEN}║     INSTALLATION COMPLETE!                           ║${NC}"
echo -e "${GREEN}║                                                      ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  ${BOLD}Plugin locations:${NC}"
echo -e "    VST3  → ~/Library/Audio/Plug-Ins/VST3/"
echo -e "    AU    → ~/Library/Audio/Plug-Ins/Components/"
echo -e "    CLAP  → ~/Library/Audio/Plug-Ins/CLAP/"
echo -e "    App   → /Applications/Clonada.app"
echo ""
echo -e "  ${BOLD}AI Engine:${NC}"
echo -e "    Home  → ~/Clonada/"
echo -e "    Start → ~/Clonada/start_engine.sh"
echo ""
echo -e "  ${BOLD}Next steps:${NC}"
echo -e "    1. Open your DAW and rescan plugins"
echo -e "    2. Load Clonada on a vocal track"
echo -e "    3. Enter your license key in the plugin"
echo -e "    4. AI models download automatically on first use (~200 MB)"
echo ""
echo -e "  ${BOLD}To uninstall:${NC} bash ~/Clonada/uninstall.sh"
echo ""
