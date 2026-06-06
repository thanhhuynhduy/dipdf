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

export VERSION
export ARCH
export NO_STRIP=1
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="/usr/lib/qt6/bin/qmake6"
export QML_SOURCES_PATHS="$PWD"
export EXTRA_QT_MODULES="svg;"
export EXTRA_PLATFORM_PLUGINS="libqxcb.so"

./tools/linuxdeploy-x86_64.AppImage \
  --appdir AppDir \
  --executable "AppDir/usr/bin/$BIN_NAME" \
  --desktop-file "AppDir/usr/share/applications/DiPDF.desktop" \
  --icon-file "AppDir/usr/share/icons/hicolor/256x256/apps/DiPDF.png" \
  --plugin qt \
  --output appimage
