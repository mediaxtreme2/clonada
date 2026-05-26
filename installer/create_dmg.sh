#!/bin/bash
# Clonada AI Vocal Suite - macOS DMG Creator
# Creates a distributable disk image with plugin components

set -e

VERSION="1.0.0"
APP_NAME="Clonada"
DMG_NAME="${APP_NAME}-${VERSION}-macOS"
BUILD_DIR="../build/Clonada_artefacts/Release"
STAGING_DIR="/tmp/clonada-dmg-staging"
DMG_STAGING="/tmp/clonada-dmg-volume"

echo "============================================================"
echo "  Clonada ${VERSION} - macOS DMG Builder"
echo "============================================================"
echo ""

# Clean previous staging
rm -rf "$STAGING_DIR" "$DMG_STAGING"
mkdir -p "$STAGING_DIR"
mkdir -p "$DMG_STAGING"

# Verify build artifacts exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "[ERROR] Build directory not found: $BUILD_DIR"
    echo "        Run cmake --build first."
    exit 1
fi

echo "[1/5] Copying plugin components..."

# VST3
if [ -d "$BUILD_DIR/VST3/Clonada.vst3" ]; then
    cp -R "$BUILD_DIR/VST3/Clonada.vst3" "$STAGING_DIR/"
    echo "  - VST3 bundle copied"
fi

# AU (Audio Unit)
if [ -d "$BUILD_DIR/AU/Clonada.component" ]; then
    cp -R "$BUILD_DIR/AU/Clonada.component" "$STAGING_DIR/"
    echo "  - Audio Unit copied"
fi

# CLAP
if [ -f "$BUILD_DIR/CLAP/Clonada.clap" ]; then
    cp "$BUILD_DIR/CLAP/Clonada.clap" "$STAGING_DIR/"
    echo "  - CLAP plugin copied"
fi

# Standalone
if [ -d "$BUILD_DIR/Standalone/Clonada.app" ]; then
    cp -R "$BUILD_DIR/Standalone/Clonada.app" "$STAGING_DIR/"
    echo "  - Standalone app copied"
fi

echo "[2/5] Creating install script..."

cat > "$STAGING_DIR/Install Clonada.command" << 'INSTALL_SCRIPT'
#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "============================================================"
echo "  Installing Clonada AI Vocal Suite"
echo "============================================================"
echo ""

# VST3
if [ -d "$SCRIPT_DIR/Clonada.vst3" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/VST3"
    mkdir -p "$DEST"
    cp -R "$SCRIPT_DIR/Clonada.vst3" "$DEST/"
    echo "[OK] VST3 installed to $DEST"
fi

# Audio Unit
if [ -d "$SCRIPT_DIR/Clonada.component" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/Components"
    mkdir -p "$DEST"
    cp -R "$SCRIPT_DIR/Clonada.component" "$DEST/"
    echo "[OK] Audio Unit installed to $DEST"
fi

# CLAP
if [ -f "$SCRIPT_DIR/Clonada.clap" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/CLAP"
    mkdir -p "$DEST"
    cp "$SCRIPT_DIR/Clonada.clap" "$DEST/"
    echo "[OK] CLAP installed to $DEST"
fi

# Standalone
if [ -d "$SCRIPT_DIR/Clonada.app" ]; then
    cp -R "$SCRIPT_DIR/Clonada.app" /Applications/
    echo "[OK] Standalone app installed to /Applications"
fi

# Create Clonada support directory
CLONADA_DIR="$HOME/Library/Application Support/Clonada"
mkdir -p "$CLONADA_DIR/Models"
mkdir -p "$CLONADA_DIR/Presets"

echo ""
echo "============================================================"
echo "  Installation complete!"
echo ""
echo "  Plugin locations:"
echo "    VST3: ~/Library/Audio/Plug-Ins/VST3/"
echo "    AU:   ~/Library/Audio/Plug-Ins/Components/"
echo "    CLAP: ~/Library/Audio/Plug-Ins/CLAP/"
echo ""
echo "  Please rescan plugins in your DAW."
echo "============================================================"
INSTALL_SCRIPT
chmod +x "$STAGING_DIR/Install Clonada.command"

echo "[3/5] Adding documentation..."

cat > "$STAGING_DIR/README.txt" << 'README'
CLONADA AI VOCAL SUITE v1.0.0
==============================

Quick Start:
1. Double-click "Install Clonada.command" to install plugins
2. Open your DAW and rescan plugins
3. Load Clonada on a vocal track
4. Enter your license key in the License panel
5. Select a voice model and start transforming!

Plugin Formats:
- VST3 (Ableton, FL Studio, Cubase, etc.)
- Audio Unit (Logic Pro, GarageBand)
- CLAP (Bitwig, Reaper)
- Standalone application

AI Engine:
The Python AI engine runs as a sidecar process. Install it by running:
  ~/Clonada/install_macos.sh

For GPU acceleration on Apple Silicon, PyTorch MPS is used automatically.

Support: https://github.com/anirudhatalmale6-alt/clonada
README

echo "[4/5] Creating DMG..."

# Create temporary DMG
hdiutil create -size 200m -fs HFS+ -volname "$APP_NAME" "$DMG_STAGING/temp.dmg"

# Mount it
MOUNT_DIR=$(hdiutil attach "$DMG_STAGING/temp.dmg" -nobrowse | tail -n1 | awk '{print $NF}')

# Copy files
cp -R "$STAGING_DIR"/* "$MOUNT_DIR/"

# Create Applications symlink for drag-install of standalone
if [ -d "$STAGING_DIR/Clonada.app" ]; then
    ln -s /Applications "$MOUNT_DIR/Applications"
fi

# Set background and icon positions (optional, cosmetic)
echo '
   tell application "Finder"
     tell disk "'$APP_NAME'"
       open
       set current view of container window to icon view
       set toolbar visible of container window to false
       set statusbar visible of container window to false
       set the bounds of container window to {100, 100, 640, 480}
       set viewOptions to the icon view options of container window
       set arrangement of viewOptions to not arranged
       set icon size of viewOptions to 80
       close
     end tell
   end tell
' | osascript || true

# Unmount
hdiutil detach "$MOUNT_DIR"

echo "[5/5] Compressing final DMG..."

# Convert to compressed DMG
hdiutil convert "$DMG_STAGING/temp.dmg" -format UDZO -o "${DMG_NAME}.dmg"

# Clean up
rm -rf "$STAGING_DIR" "$DMG_STAGING"

echo ""
echo "============================================================"
echo "  DMG created: ${DMG_NAME}.dmg"
echo "============================================================"
