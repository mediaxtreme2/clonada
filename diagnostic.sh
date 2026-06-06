#!/bin/bash
# Clonada v1.7.5 Diagnostic Script
# Paste this entire block into Terminal and hit Enter

echo "============================================"
echo "  CLONADA v1.7.5 DIAGNOSTIC REPORT"
echo "============================================"
echo ""
echo "macOS Version: $(sw_vers -productVersion)"
echo "Architecture: $(uname -m)"
echo "Date: $(date)"
echo ""

# Step 1: Remove ALL old versions
echo "--- STEP 1: Cleaning old installations ---"
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Clonada.vst3" 2>/dev/null
sudo rm -rf "/Library/Audio/Plug-Ins/Components/Clonada.component" 2>/dev/null
sudo rm -rf "/Library/Audio/Plug-Ins/CLAP/Clonada.clap" 2>/dev/null
sudo rm -rf "/Applications/Clonada.app" 2>/dev/null
sudo rm -f /usr/local/bin/clonada-engine 2>/dev/null
rm -rf ~/Library/Audio/Plug-Ins/VST3/Clonada.vst3 2>/dev/null
rm -rf ~/Library/Audio/Plug-Ins/Components/Clonada.component 2>/dev/null
rm -rf ~/Library/Caches/AudioUnitCache 2>/dev/null
rm -rf ~/Library/Caches/com.apple.audiounits.cache 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Old files removed."

# Step 2: Download v1.7.5
echo ""
echo "--- STEP 2: Downloading Clonada v1.7.5 ---"
cd /tmp
curl -sL "https://github.com/mediaxtreme2/clonada/releases/download/v1.7.5/Clonada-1.7.5-macOS.pkg" -o Clonada-1.7.5.pkg
echo "Downloaded: $(ls -lh Clonada-1.7.5.pkg | awk '{print $5}')"

# Step 3: Check pkg signature
echo ""
echo "--- STEP 3: Package Signature ---"
pkgutil --check-signature Clonada-1.7.5.pkg

# Step 4: Install
echo ""
echo "--- STEP 4: Installing ---"
sudo installer -pkg Clonada-1.7.5.pkg -target / -verbose

# Step 5: Verify files
echo ""
echo "--- STEP 5: Checking installed files ---"
echo -n "Clonada.app: "
[ -d "/Applications/Clonada.app" ] && echo "FOUND" || echo "MISSING"
echo -n "VST3: "
[ -d "/Library/Audio/Plug-Ins/VST3/Clonada.vst3" ] && echo "FOUND" || echo "MISSING"
echo -n "AU Component: "
[ -d "/Library/Audio/Plug-Ins/Components/Clonada.component" ] && echo "FOUND" || echo "MISSING"
echo -n "Cloud Engine: "
[ -f "/usr/local/bin/clonada-engine" ] && echo "FOUND" || echo "MISSING"

# Step 6: Check versions
echo ""
echo "--- STEP 6: Version Check ---"
if [ -f "/Library/Audio/Plug-Ins/VST3/Clonada.vst3/Contents/Info.plist" ]; then
    echo -n "VST3 Version: "
    defaults read "/Library/Audio/Plug-Ins/VST3/Clonada.vst3/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "N/A"
fi
if [ -f "/Library/Audio/Plug-Ins/Components/Clonada.component/Contents/Info.plist" ]; then
    echo -n "AU Version: "
    defaults read "/Library/Audio/Plug-Ins/Components/Clonada.component/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "N/A"
fi
if [ -f "/Applications/Clonada.app/Contents/Info.plist" ]; then
    echo -n "App Version: "
    defaults read "/Applications/Clonada.app/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "N/A"
fi

# Step 7: Code signing
echo ""
echo "--- STEP 7: Code Signing ---"
codesign --verify --deep --strict "/Applications/Clonada.app" 2>&1 && echo "App: VALID" || echo "App: INVALID or MISSING"
codesign --verify --deep --strict "/Library/Audio/Plug-Ins/VST3/Clonada.vst3" 2>&1 && echo "VST3: VALID" || echo "VST3: INVALID or MISSING"

# Step 8: Try to open
echo ""
echo "--- STEP 8: Launch Test ---"
echo "Attempting to open Clonada.app..."
open -a "/Applications/Clonada.app" 2>&1 && echo "App launched (check if window appeared)" || echo "App FAILED to launch"

echo ""
echo "============================================"
echo "  DIAGNOSTIC COMPLETE"
echo "  Copy all output above and send it back"
echo "============================================"
