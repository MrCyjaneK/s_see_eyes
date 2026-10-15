#!/usr/bin/env bash
# One-time fetch of bundled ONNX models (build/setup only — load() never downloads).
set -euo pipefail
cd "$(dirname "$0")/.."
MODELS="models"
VENV=".venv"
mkdir -p "$MODELS"

ensure_venv() {
  if [[ ! -d "$VENV" ]]; then
    echo "[+] creating $VENV"
    python3 -m venv "$VENV"
  fi
  # shellcheck disable=SC1091
  source "$VENV/bin/activate"
}

fetch() {
  local name="$1" url="$2"
  if [[ -f "$MODELS/$name" && -s "$MODELS/$name" ]]; then
    echo "[=] $name"
    return 0
  fi
  echo "[+] $name"
  curl -fsSL -L "$url" -o "$MODELS/$name"
}

fetch yolo26n.onnx \
  "https://github.com/ultralytics/assets/releases/download/v8.4.0/yolo26n.onnx" || true
if [[ ! -f "$MODELS/yolo26n.onnx" ]]; then
  echo "[+] exporting yolo26n.onnx via ultralytics"
  ensure_venv
  pip install ultralytics onnx onnxruntime -q
  python3 - <<'PY'
from ultralytics import YOLO
import shutil
YOLO('yolo26n.pt').export(format='onnx', imgsz=640, simplify=True)
shutil.copy('yolo26n.onnx', 'models/yolo26n.onnx')
PY
fi

RAPID_OCR="https://www.modelscope.cn/models/RapidAI/RapidOCR/resolve/v3.9.0/onnx"
fetch ppocr_det.onnx "$RAPID_OCR/PP-OCRv6/det/PP-OCRv6_det_small.onnx"
fetch ppocr_rec.onnx "$RAPID_OCR/PP-OCRv6/rec/PP-OCRv6_rec_small.onnx"
fetch ppocr_cls.onnx "$RAPID_OCR/PP-OCRv4/cls/ch_ppocr_mobile_v2.0_cls_mobile.onnx"
ensure_venv
pip install onnxruntime -q
python3 - <<'PY'
import onnxruntime as ort
from pathlib import Path
dst = Path("models")
meta = ort.InferenceSession(str(dst / "ppocr_rec.onnx"), providers=["CPUExecutionProvider"]).get_modelmeta().custom_metadata_map
keys = meta.get("character", "")
(dst / "ppocr_keys.txt").write_text(keys.replace("\r\n", "\n"))
print("[+] ppocr_keys.txt")
PY
echo "[+] OCR models"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
if [[ ! -f "$MODELS/scrfd_det.onnx" || ! -f "$MODELS/w600k_mbf.onnx" ]]; then
  curl -fsSL -L "https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_sc.zip" -o "$TMP/buffalo.zip"
  if [[ ! -f "$MODELS/scrfd_det.onnx" ]]; then
    unzip -qo "$TMP/buffalo.zip" det_500m.onnx -d "$TMP"
    mv "$TMP/det_500m.onnx" "$MODELS/scrfd_det.onnx"
    echo "[+] scrfd_det.onnx"
  else
    echo "[=] scrfd_det.onnx"
  fi
  if [[ ! -f "$MODELS/w600k_mbf.onnx" ]]; then
    unzip -qo "$TMP/buffalo.zip" w600k_mbf.onnx -d "$TMP"
    mv "$TMP/w600k_mbf.onnx" "$MODELS/w600k_mbf.onnx"
    echo "[+] w600k_mbf.onnx"
  else
    echo "[=] w600k_mbf.onnx"
  fi
else
  echo "[=] scrfd_det.onnx"
  echo "[=] w600k_mbf.onnx"
fi

fetch mobilenet_tags.onnx \
  "https://github.com/onnx/models/raw/refs/heads/main/validated/vision/classification/mobilenet/model/mobilenetv2-12.onnx"
fetch imagenet_labels.txt \
  "https://raw.githubusercontent.com/pytorch/hub/master/imagenet_classes.txt"

HF_XENOVA="https://huggingface.co/Xenova/vit-gpt2-image-captioning/resolve/main"
fetch caption_encoder.onnx "$HF_XENOVA/onnx/encoder_model.onnx"
fetch caption_decoder.onnx "$HF_XENOVA/onnx/decoder_model.onnx"
fetch caption_tokenizer.json "$HF_XENOVA/tokenizer.json"
fetch caption_config.json "$HF_XENOVA/config.json"

echo "[+] Models in $MODELS ($(du -sh "$MODELS" | awk '{print $1}'))"
ls -lh "$MODELS"
python3 scripts/gen_assets.py "$MODELS" src/assets.cpp
