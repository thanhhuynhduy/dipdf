#!/usr/bin/env bash
set -euo pipefail

BIN_NAME="DiPDF"
VERSION="${VERSION:-1.0.0}"
ARCH="${ARCH:-x86_64}"

rm -rf build AppDir tools *.AppImage
mkdir -p tools

wget -O tools/linuxdeploy-x86_64.AppImage \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage

wget -O tools/linuxdeploy-plugin-qt-x86_64.AppImage \
  https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

chmod +x tools/*.AppImage

cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build build

DESTDIR="$PWD/AppDir" cmake --install build

# Some CI builds can still succeed at compiling while CMake has no/partial
# install rules. Make the AppDir layout explicit so linuxdeploy always finds
# the executable, desktop file, and icon.
mkdir -p AppDir/usr/bin          AppDir/usr/share/applications          AppDir/usr/share/icons/hicolor/256x256/apps

if [[ ! -x "AppDir/usr/bin/$BIN_NAME" ]]; then
  if [[ -x "build/$BIN_NAME" ]]; then
    install -Dm755 "build/$BIN_NAME" "AppDir/usr/bin/$BIN_NAME"
  else
    echo "ERROR: Cannot find built executable build/$BIN_NAME" >&2
    find build -maxdepth 3 -type f -perm -111 -print >&2 || true
    exit 1
  fi
fi

if [[ ! -f "AppDir/usr/share/applications/DiPDF.desktop" && -f "packaging/DiPDF.desktop" ]]; then
  install -Dm644 "packaging/DiPDF.desktop" "AppDir/usr/share/applications/DiPDF.desktop"
fi

if [[ ! -f "AppDir/usr/share/icons/hicolor/256x256/apps/DiPDF.png" && -f "packaging/DiPDF.png" ]]; then
  install -Dm644 "packaging/DiPDF.png" "AppDir/usr/share/icons/hicolor/256x256/apps/DiPDF.png"
fi

export VERSION
export ARCH
export NO_STRIP=1
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="/usr/lib/qt6/bin/qmake6"
# DiPDF is a Qt Widgets app. Do not ask linuxdeploy's Qt plugin to scan the
# whole repository for QML files, otherwise stale/example QML can break CI.
unset QML_SOURCES_PATHS
export EXTRA_QT_MODULES="svg;"
export EXTRA_PLATFORM_PLUGINS="libqxcb.so"

# We run linuxdeploy without --output appimage first to generate the AppDir plugin layout
./tools/linuxdeploy-x86_64.AppImage \
  --appdir AppDir \
  --executable "AppDir/usr/bin/$BIN_NAME" \
  --desktop-file "AppDir/usr/share/applications/DiPDF.desktop" \
  --icon-file "AppDir/usr/share/icons/hicolor/256x256/apps/DiPDF.png" \
  --plugin qt

echo "── Inspecting AppDir plugin layout ──"
find AppDir -name 'libqxcb.so'

# ── Bundle fcitx5 input-context plugin and dependencies ──
FCITX_PLUGIN=$(find /usr -name 'libfcitx5platforminputcontextplugin.so' 2>/dev/null | head -1)
if [[ -z "$FCITX_PLUGIN" ]]; then
  # Fallback to broader wildcard
  FCITX_PLUGIN=$(find /usr -path '*platforminputcontexts*' -name '*fcitx*.so' 2>/dev/null | head -1)
fi

if [[ -n "$FCITX_PLUGIN" ]]; then
  # Determine where linuxdeploy placed Qt plugins by finding libqxcb.so
  QCB_PATH=$(find AppDir -name 'libqxcb.so' | head -n 1)
  if [[ -n "$QCB_PATH" ]]; then
    # e.g. AppDir/usr/lib/qt6/plugins/platforms/libqxcb.so -> AppDir/usr/lib/qt6/plugins/platforminputcontexts
    DEST_DIR="$(dirname $(dirname "$QCB_PATH"))/platforminputcontexts"
  else
    # Fallback
    DEST_DIR="AppDir/usr/plugins/platforminputcontexts"
  fi

  mkdir -p "$DEST_DIR"
  cp -v "$FCITX_PLUGIN" "$DEST_DIR/"
  echo "[DiPDF] Bundled fcitx plugin: $FCITX_PLUGIN -> $DEST_DIR"

  # Bundle dependencies using ldd
  echo "[DiPDF] Bundling fcitx dependencies..."
  DEPS=$(ldd "$DEST_DIR/$(basename "$FCITX_PLUGIN")" | awk '{print $3}' | grep -E '/usr/lib|/lib' | grep -i 'fcitx' || true)
  for dep in $DEPS; do
    if [[ -f "$dep" ]]; then
      cp -v "$dep" "AppDir/usr/lib/"
    fi
  done
  
  # Patch RPATH of the fcitx plugin
  patchelf --set-rpath '$ORIGIN/../../lib' "$DEST_DIR/$(basename "$FCITX_PLUGIN")"
  
  # Patch RPATH of bundled fcitx libraries
  for dep in AppDir/usr/lib/libFcitx5*.so* AppDir/usr/lib/libfcitx5*.so*; do
      if [[ -f "$dep" && ! -L "$dep" ]]; then
          patchelf --set-rpath '$ORIGIN' "$dep"
      fi
  done

  echo "[DiPDF] Verifying fcitx plugin dependencies in AppDir..."
  if ldd "$DEST_DIR/$(basename "$FCITX_PLUGIN")" | grep -q "not found"; then
      echo "ERROR: Missing dependencies for fcitx plugin!"
      exit 1
  fi
  
  # Fail if ldd resolves libFcitx5Qt6DBusAddons.so.1 to /lib64 or /usr/lib instead of AppDir/usr/lib
  if ldd "$DEST_DIR/$(basename "$FCITX_PLUGIN")" | grep -q "/lib64/libFcitx5\|/usr/lib/libFcitx5\|/usr/lib/x86_64-linux-gnu/libFcitx5"; then
      echo "ERROR: fcitx dependencies are being resolved to the host system instead of AppDir/usr/lib!"
      ldd "$DEST_DIR/$(basename "$FCITX_PLUGIN")"
      exit 1
  fi
else
  echo "[DiPDF] WARNING: fcitx platforminputcontext plugin not found; Vietnamese IME may not work in AppImage"
  exit 1
fi

echo "── Final Verification ──"
find AppDir -path '*platforminputcontexts*' -print
find AppDir -name '*fcitx*' -print

FCITX_COPIED=$(find AppDir -path '*platforminputcontexts*' -name '*fcitx*.so' 2>/dev/null | head -1)
if [[ -n "$FCITX_COPIED" ]]; then
  echo "[DiPDF] Final ldd check on copied plugin:"
  ldd "$FCITX_COPIED" || true
  echo "[DiPDF] Checking RPATH of copied plugin:"
  patchelf --print-rpath "$FCITX_COPIED"
  readelf -d "$FCITX_COPIED" | grep -E 'RPATH|RUNPATH|NEEDED'
else
  echo "ERROR: libfcitx5platforminputcontextplugin.so is missing from AppDir before finalizing!"
  exit 1
fi

if [[ -f AppDir/qt.conf ]]; then
    cat AppDir/qt.conf
fi

echo "── Creating Custom AppRun ──"
rm -f AppDir/AppRun
cat > AppDir/AppRun << 'EOF'
#!/usr/bin/env bash
APPDIR="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$APPDIR/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$APPDIR/usr/plugins/platforms"

# Only fallback to fcitx if user has not explicitly set IME modules
export QT_IM_MODULE="${QT_IM_MODULE:-fcitx}"
export XMODIFIERS="${XMODIFIERS:-@im=fcitx}"
export GTK_IM_MODULE="${GTK_IM_MODULE:-fcitx}"
export SDL_IM_MODULE="${SDL_IM_MODULE:-fcitx}"

if [[ "${DIPDF_FORCE_XCB:-0}" == "1" ]]; then
    export QT_QPA_PLATFORM="xcb"
fi

exec "$APPDIR/usr/bin/DiPDF" "$@"
EOF
chmod +x AppDir/AppRun

# Finally generate the AppImage
wget -O tools/appimagetool-x86_64.AppImage https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x tools/appimagetool-x86_64.AppImage
./tools/appimagetool-x86_64.AppImage AppDir "DiPDF-$VERSION-$ARCH.AppImage"
