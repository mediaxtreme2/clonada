#!/bin/bash
set -e

# ════════════════════════════════════════════════════════════════
#   CLONADA AI VOCAL SUITE — macOS One-Click Installer
#   Cloud Edition: No Python, no downloads, instant setup
# ════════════════════════════════════════════════════════════════

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION="1.7.0"
INSTALL_DIR="$HOME/Clonada"
TOTAL_STEPS=5
CURRENT=0

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

ok() { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }
fail() { echo -e "  ${RED}✗ ERROR:${NC} $1"; exit 1; }

echo ""
echo -e "${BOLD}╔══════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}║                                                      ║${NC}"
echo -e "${BOLD}║     CLONADA AI VOCAL SUITE — macOS Installer         ║${NC}"
echo -e "${BOLD}║     Version ${VERSION} (Cloud Edition)                ║${NC}"
echo -e "${BOLD}║     by mediaXtreme LLC                               ║${NC}"
echo -e "${BOLD}║                                                      ║${NC}"
echo -e "${BOLD}╚══════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  This installer sets up everything in under 30 seconds:"
echo -e "    • Audio plugins (VST3, AU, CLAP)"
echo -e "    • Standalone application"
echo -e "    • Cloud AI engine (no downloads needed)"
echo ""

# ──────────────────────────────────────────────────────
# STEP 1: Install Audio Plugins
# ──────────────────────────────────────────────────────
step "Installing audio plugins"

PLUGINS_INSTALLED=0

if [ -d "$SCRIPT_DIR/Clonada.vst3" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$DEST"
    rm -rf "$DEST/Clonada.vst3"
    cp -R "$SCRIPT_DIR/Clonada.vst3" "$DEST/"
    ok "VST3 → ~/Library/Audio/Plug-Ins/VST3/"
    PLUGINS_INSTALLED=$((PLUGINS_INSTALLED + 1))
fi

if [ -d "$SCRIPT_DIR/Clonada.component" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$DEST"
    rm -rf "$DEST/Clonada.component"
    cp -R "$SCRIPT_DIR/Clonada.component" "$DEST/"
    ok "Audio Unit → ~/Library/Audio/Plug-Ins/Components/"
    PLUGINS_INSTALLED=$((PLUGINS_INSTALLED + 1))
fi

if [ -f "$SCRIPT_DIR/Clonada.clap" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/CLAP"
    mkdir -p "$DEST"
    rm -f "$DEST/Clonada.clap"
    cp "$SCRIPT_DIR/Clonada.clap" "$DEST/"
    ok "CLAP → ~/Library/Audio/Plug-Ins/CLAP/"
    PLUGINS_INSTALLED=$((PLUGINS_INSTALLED + 1))
fi

if [ $PLUGINS_INSTALLED -eq 0 ]; then
    warn "No plugin binaries found — skipping plugin install"
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
# STEP 3: Create directories and install cloud engine
# ──────────────────────────────────────────────────────
step "Setting up Clonada directory and cloud engine"

mkdir -p "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR/models"
mkdir -p "$HOME/Library/Application Support/Clonada/Presets"

if [ -f "$SCRIPT_DIR/clonada-engine" ]; then
    cp "$SCRIPT_DIR/clonada-engine" "$INSTALL_DIR/clonada-engine"
    chmod +x "$INSTALL_DIR/clonada-engine"
    ok "Cloud AI engine → ~/Clonada/clonada-engine"
else
    warn "Cloud engine binary not found"
fi

ok "Created ~/Clonada/ with models directory"

# ──────────────────────────────────────────────────────
# STEP 4: Create launcher
# ──────────────────────────────────────────────────────
step "Creating launcher"

cat > "$INSTALL_DIR/start_engine.sh" << 'LAUNCHER'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "Starting Clonada Cloud AI Engine..."
"$SCRIPT_DIR/clonada-engine" --models-dir "$SCRIPT_DIR/models" "$@"
LAUNCHER
chmod +x "$INSTALL_DIR/start_engine.sh"
ok "start_engine.sh created"

# Uninstaller
cat > "$INSTALL_DIR/uninstall.sh" << 'UNINSTALL'
#!/bin/bash
echo "Clonada Uninstaller"
read -p "Remove Clonada and all data? (y/N) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Clonada.vst3"
    rm -rf "$HOME/Library/Audio/Plug-Ins/Components/Clonada.component"
    rm -rf "$HOME/Library/Audio/Plug-Ins/CLAP/Clonada.clap"
    rm -rf "/Applications/Clonada.app"
    rm -rf "$HOME/Library/Application Support/Clonada"
    rm -rf "$HOME/Clonada"
    echo "Clonada removed."
fi
UNINSTALL
chmod +x "$INSTALL_DIR/uninstall.sh"
ok "uninstall.sh created"

# ──────────────────────────────────────────────────────
# STEP 5: Remove quarantine attributes
# ──────────────────────────────────────────────────────
step "Removing quarantine flags"

xattr -rd com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Clonada.vst3" 2>/dev/null || true
xattr -rd com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/Clonada.component" 2>/dev/null || true
xattr -rd com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/CLAP/Clonada.clap" 2>/dev/null || true
xattr -rd com.apple.quarantine /Applications/Clonada.app 2>/dev/null || true
xattr -rd com.apple.quarantine "$INSTALL_DIR/clonada-engine" 2>/dev/null || true
ok "Quarantine flags cleared"

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
echo -e "    Mode  → Cloud GPU (no local setup needed)"
echo -e "    Home  → ~/Clonada/"
echo ""
echo -e "  ${BOLD}Next steps:${NC}"
echo -e "    1. Open your DAW and rescan plugins"
echo -e "    2. Load Clonada on a vocal track"
echo -e "    3. Enter your license key in the plugin"
echo -e "    4. Start converting! (cloud processing, no local GPU needed)"
echo ""
echo -e "  ${BOLD}To uninstall:${NC} bash ~/Clonada/uninstall.sh"
echo ""
