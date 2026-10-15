#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
IMG="testdata/images"
mkdir -p "$IMG"

fetch_img() {
  local name="$1" url="$2"
  if [[ -f "$IMG/$name" ]]; then return 0; fi
  echo "[+] $name"
  curl -fsSL -A "eyes-test/1.0" "$url" -o "$IMG/$name"
}

fetch_img street.jpg "https://raw.githubusercontent.com/ultralytics/yolov5/master/data/images/bus.jpg"
fetch_img faces.jpg "https://raw.githubusercontent.com/ultralytics/yolov5/master/data/images/zidane.jpg"

if [[ ! -f "$IMG/street.heic" ]]; then
  echo "[+] street.heic (converted from street.jpg)"
  ffmpeg -i "$IMG/street.jpg" "$IMG/street.heic" >/dev/null
fi

if [[ ! -f "$IMG/text.png" ]]; then
  echo "[+] text.png (generated)"
  python3 - <<'PY'
from PIL import Image, ImageDraw
img = Image.new("RGB", (480, 120), "white")
draw = ImageDraw.Draw(img)
draw.text((20, 40), "Hello OCR 12345", fill="black")
img.save("testdata/images/text.png")
PY
fi

echo "[+] Test images in $IMG"