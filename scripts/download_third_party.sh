#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TP="third_party/src"
mkdir -p "$TP/nlohmann"

fetch() {
  local name="$1" url="$2"
  if [[ -f "$TP/$name" ]]; then return 0; fi
  echo "[+] $name"
  curl -fsSL "$url" -o "$TP/$name"
}

fetch nlohmann/json.hpp "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
fetch stb_image.h "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
fetch easyexif.h "https://raw.githubusercontent.com/mayanklahiri/easyexif/master/exif.h"
fetch easyexif.cpp "https://raw.githubusercontent.com/mayanklahiri/easyexif/master/exif.cpp"
cp -f "$TP/easyexif.h" "$TP/exif.h"

echo "[+] Third-party headers in $TP"
