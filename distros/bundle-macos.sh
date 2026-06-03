#!/bin/bash
# Bundle ANUGA Viewer into a macOS .app and wrap it in a DMG.
# Run from the repo root, or called by "make dmg".
# Requires: dylibbundler, create-dmg  (brew install dylibbundler create-dmg)
set -e

BREW_PREFIX="${1:-$(brew --prefix 2>/dev/null || echo /usr/local)}"
CURDIR="$(cd "$(dirname "$0")/.." && pwd)"

APP="ANUGA Viewer.app"
MACOS="$APP/Contents/MacOS"
LIBS_DIR="$APP/Contents/libs"

echo "=== Bundling $APP ==="
rm -rf "$APP"
mkdir -p "$MACOS/fonts" "$MACOS/images" "$LIBS_DIR" "$APP/Contents/Resources"

cp "$CURDIR/bin/anuga_viewer"         "$MACOS/"
cp "$CURDIR/bin/fonts/arial.ttf"      "$MACOS/fonts/"
cp "$CURDIR/bin/images/sky_small.jpg" "$MACOS/images/"
cp "$CURDIR/bin/images/envmap.jpg"    "$MACOS/images/"
cp "$CURDIR/distros/icon.png"         "$APP/Contents/Resources/"

cat > "$APP/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleName</key>        <string>ANUGA Viewer</string>
  <key>CFBundleExecutable</key>  <string>anuga_viewer</string>
  <key>CFBundleIdentifier</key>  <string>au.gov.ga.anuga-viewer</string>
  <key>CFBundleIconFile</key>    <string>icon.png</string>
  <key>CFBundlePackageType</key> <string>APPL</string>
  <key>CFBundleVersion</key>     <string>1.0</string>
</dict></plist>
EOF

# Fix the dylib's install name and the viewer's recorded reference to an
# absolute path so dylibbundler can locate and read the dylib's own deps.
SWWREADER_ABS="$CURDIR/bin/libswwreader.dylib"
install_name_tool -id "$SWWREADER_ABS" "$CURDIR/bin/libswwreader.dylib"
install_name_tool -change "../bin/libswwreader.dylib" "$SWWREADER_ABS" \
    "$MACOS/anuga_viewer"

cp "$CURDIR/bin/libswwreader.dylib" "$LIBS_DIR/"
dylibbundler \
    --overwrite-dir \
    --bundle-deps \
    --fix-file "$MACOS/anuga_viewer" \
    --dest-dir "$LIBS_DIR" \
    --install-path "@executable_path/../libs" \
    --search-path "$BREW_PREFIX/lib" \
    --search-path "$CURDIR/bin"

echo "=== Creating DMG ==="
create-dmg \
    --volname "ANUGA Viewer" \
    --window-pos 200 120 \
    --window-size 600 300 \
    --icon-size 100 \
    --icon "ANUGA Viewer.app" 175 120 \
    --hide-extension "ANUGA Viewer.app" \
    --app-drop-link 425 120 \
    "$CURDIR/anuga-viewer-macos.dmg" \
    "$APP"

echo "=== Done: $CURDIR/anuga-viewer-macos.dmg ==="
du -sh "$CURDIR/anuga-viewer-macos.dmg"
