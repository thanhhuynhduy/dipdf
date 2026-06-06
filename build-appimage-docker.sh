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

./tools/linuxdeploy-x86_64.AppImage \
  --appdir AppDir \
  --executable "AppDir/usr/bin/$BIN_NAME" \
  --desktop-file "AppDir/usr/share/applications/DiPDF.desktop" \
  --icon-file "AppDir/usr/share/icons/hicolor/256x256/apps/DiPDF.png" \
  --plugin qt \
  --output appimage
