#!/bin/bash
# Bundle ANUGA Viewer into a self-contained Windows directory and build
# an NSIS installer.  Run from MSYS2 MINGW64 shell, or called by "make nsis".
# Requires: MSYS2 packages mingw-w64-x86_64-{gcc,OpenSceneGraph,netcdf,curl}
#           NSIS installed via: choco install nsis
set -e

VERSION="${1:-dev}"
CURDIR="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$CURDIR/dist/anuga-viewer"
NSIS="${NSIS:-C:/Program Files (x86)/NSIS/makensis.exe}"

echo "=== Building bundle in $DIST ==="
rm -rf "$DIST"
mkdir -p "$DIST/fonts" "$DIST/images"

cp "$CURDIR/bin/anuga_viewer.exe"     "$DIST/"
cp "$CURDIR/bin/fonts/arial.ttf"      "$DIST/fonts/"
cp "$CURDIR/bin/images/sky_small.jpg" "$DIST/images/"
cp "$CURDIR/bin/images/envmap.jpg"    "$DIST/images/"
cp "$CURDIR/bin/libswwreader.dll"     "$DIST/" 2>/dev/null || true

# OSG plugins — whitelist only what anuga-viewer needs
PLUGIN_DIR=$(find /mingw64 -maxdepth 6 -type d -name 'osgPlugins*' 2>/dev/null | head -1)
if [ -n "$PLUGIN_DIR" ]; then
    DEST_PLUGINS="$DIST/$(basename "$PLUGIN_DIR")"
    mkdir -p "$DEST_PLUGINS"
    for p in mingw_osgdb_jpeg mingw_osgdb_png mingw_osgdb_rgb \
              mingw_osgdb_tga mingw_osgdb_bmp mingw_osgdb_dds \
              mingw_osgdb_tiff mingw_osgdb_freetype \
              mingw_osgdb_osg mingw_osgdb_ive mingw_osgdb_deprecated_osg; do
        [ -f "$PLUGIN_DIR/${p}.dll" ] && cp "$PLUGIN_DIR/${p}.dll" "$DEST_PLUGINS/" \
            && echo "  bundled: ${p}.dll"
    done
    echo "Plugins bundled: $(ls "$DEST_PLUGINS/" | wc -l)"
else
    echo "WARNING: no osgPlugins directory found"
fi

# Iteratively bundle all MinGW DLL dependencies until no new ones are added
echo "=== Bundling DLLs ==="
for i in 1 2 3 4 5; do
    BEFORE=$(find "$DIST" -name "*.dll" | wc -l)
    find "$DIST" \( -name "*.dll" -o -name "*.exe" \) \
        | xargs ldd 2>/dev/null \
        | grep -i mingw64 | awk '{print $3}' | sort -u \
        | xargs -I{} cp -n "{}" "$DIST/" 2>/dev/null || true
    AFTER=$(find "$DIST" -name "*.dll" | wc -l)
    echo "  pass $i: $BEFORE -> $AFTER DLLs"
    [ "$AFTER" = "$BEFORE" ] && break
done

# Minimal Fontconfig config to suppress "Cannot load default config file" warnings
mkdir -p "$DIST/etc/fonts"
echo '<fontconfig></fontconfig>' > "$DIST/etc/fonts/fonts.conf"

# CA certificate bundle for libcurl HTTPS verification
for f in /mingw64/ssl/certs/ca-bundle.crt \
          /mingw64/etc/ssl/certs/ca-bundle.crt \
          /usr/ssl/certs/ca-bundle.crt; do
    [ -f "$f" ] && cp "$f" "$DIST/ca-bundle.crt" && echo "CA bundle: $f" && break
done

# Launcher batch file
cat > "$DIST/run_viewer.bat" << 'BATCH'
@echo off
set SWOLLEN_BINDIR=%~dp0
set OSG_LIBRARY_PATH=%~dp0
set FONTCONFIG_FILE=%~dp0etc\fonts\fonts.conf
set CURL_CA_BUNDLE=%~dp0ca-bundle.crt
"%~dp0anuga_viewer.exe" %*
BATCH

echo "=== Bundle size ===" && du -sh "$DIST" && du -sh "$DIST"/* | sort -rh | head -10

echo "=== Building NSIS installer (version: $VERSION) ==="
"$NSIS" -DVERSION="$VERSION" "$CURDIR/installer/anuga_viewer.nsi"
echo "=== Done: $CURDIR/anuga-viewer-windows-setup.exe ==="
