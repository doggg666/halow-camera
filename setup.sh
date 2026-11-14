#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sdk="$root/mm-iot-esp32"
if [ ! -d "$sdk/.git" ]; then
    git clone https://github.com/Seeed-Studio/mm-iot-esp32.git "$sdk"
fi
git -C "$sdk" fetch --depth 1 origin b8b7452ddbe2cc9b7a4d94e22ae6f4653c596821
git -C "$sdk" checkout --detach b8b7452ddbe2cc9b7a4d94e22ae6f4653c596821
cp -a "$root/overlay/." "$sdk/"
printf 'Seeed HaLow camera source is ready in %s\n' "$sdk"
