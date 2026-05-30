#!/bin/bash
# Clonada AI Vocal Suite - macOS DMG Creator
# Creates a distributable disk image with unified single-step installer

set -e

VERSION="1.2.0"
APP_NAME="Clonada"
DMG_NAME="${APP_NAME}-${VERSION}-macOS"
BUILD_DIR="../plugin/build/Clonada_artefacts/Release"
STAGING_DIR="/tmp/clonada-dmg-staging"
DMG_STAGING="/tmp/clonada-dmg-volume"

echo "============================================================"
echo "  Clonada ${VERSION} - macOS DMG Builder"
echo "============================================================"
echo ""

rm -rf "$STAGING_DIR" "$DMG_STAGING"
mkdir -p "$STAGING_DIR" "$DMG_STAGING" "$STAGING_DIR/python/lib"

if [ ! -d "$BUILD_DIR" ]; then
    echo "[ERROR] Build directory not found: $BUILD_DIR"
    echo "        Run cmake --build first."
    exit 1
fi

echo "[1/5] Copying plugin components..."

[ -d "$BUILD_DIR/VST3/Clonada.vst3" ] && cp -R "$BUILD_DIR/VST3/Clonada.vst3" "$STAGING_DIR/" && echo "  - VST3 bundle copied"
[ -d "$BUILD_DIR/AU/Clonada.component" ] && cp -R "$BUILD_DIR/AU/Clonada.component" "$STAGING_DIR/" && echo "  - Audio Unit copied"
[ -f "$BUILD_DIR/CLAP/Clonada.clap" ] && cp "$BUILD_DIR/CLAP/Clonada.clap" "$STAGING_DIR/" && echo "  - CLAP plugin copied"
[ -d "$BUILD_DIR/Standalone/Clonada.app" ] && cp -R "$BUILD_DIR/Standalone/Clonada.app" "$STAGING_DIR/" && echo "  - Standalone app copied"

echo "[2/5] Copying Python engine..."
cp ../python/*.py "$STAGING_DIR/python/"
cp ../python/requirements.txt "$STAGING_DIR/python/"
cp ../python/lib/*.py "$STAGING_DIR/python/lib/"
echo "  - Python engine files copied"

echo "[3/5] Setting up unified installer..."

# Copy the unified installer script
cp install_macos.sh "$STAGING_DIR/"
chmod +x "$STAGING_DIR/install_macos.sh"

# Create the .command wrapper for Finder double-click
cat > "$STAGING_DIR/Install Clonada.command" << 'CMDEOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
bash "$SCRIPT_DIR/install_macos.sh"
echo ""
echo "You can close this window now."
read -n 1 -s -r -p "Press any key to exit..."
CMDEOF
chmod +x "$STAGING_DIR/Install Clonada.command"

cp license.txt "$STAGING_DIR/"

cat > "$STAGING_DIR/README.txt" << README
CLONADA AI VOCAL SUITE v${VERSION}
============================================

ONE-STEP INSTALL:
  Double-click "Install Clonada.command"

That's it! The installer handles everything:
  - Copies plugins (VST3, AU, CLAP) to your DAW folders
  - Installs standalone app to Applications
  - Sets up the Python AI engine
  - Creates all necessary directories

After installation:
  1. Open your DAW and rescan plugins
  2. Load Clonada on a vocal track
  3. Enter your license key
  4. AI models download automatically on first use

To uninstall: bash ~/Clonada/uninstall.sh
README

echo "[4/5] Creating DMG..."

hdiutil create -volname "$APP_NAME" -srcfolder "$STAGING_DIR" -ov -format UDZO "${DMG_NAME}.dmg"

echo "[5/5] Cleanup..."
rm -rf "$STAGING_DIR" "$DMG_STAGING"

echo ""
echo "============================================================"
echo "  DMG created: ${DMG_NAME}.dmg"
echo "============================================================"
