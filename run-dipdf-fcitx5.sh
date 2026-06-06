#!/usr/bin/env bash
# Run DiPDF with fcitx5 Vietnamese input enabled for Qt6.
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export GTK_IM_MODULE=fcitx
export SDL_IM_MODULE=fcitx
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "$DIR/build/DiPDF" ]]; then
  exec "$DIR/build/DiPDF" "$@"
elif [[ -x "$DIR/DiPDF" ]]; then
  exec "$DIR/DiPDF" "$@"
else
  echo "Build the app first, then run this script from the project folder." >&2
  echo "Example: cmake -S . -B build && cmake --build build" >&2
  exit 1
fi
