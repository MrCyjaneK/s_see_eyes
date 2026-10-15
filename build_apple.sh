#!/usr/bin/env bash
# Build eyes.xcframework + onnxruntime.xcframework for iOS device, iOS simulator, and macOS.
#
# libeyes dlopens onnxruntime.framework. It does not link the dylib.
#
# Prereqs (run separately, once):
#   make simplybs   # or per-triplet simplybs-<triplet> for eyes packages
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
# shellcheck source=../apple.sh
source "$SCRIPT_DIR/../apple.sh"

ORT_DYLIB_NAME=""

ort_dylib_in() {
  local prefix="$1"
  local f
  for f in "$prefix"/lib/libonnxruntime.*.dylib; do
    [[ -f "$f" && ! -L "$f" ]] || continue
    echo "$f"
    return 0
  done
  return 1
}

stage_ort() {
  local src="$1" dest_dir="$2"
  mkdir -p "$dest_dir"
  ORT_DYLIB_NAME="$(basename "$src")"
  cp -f "$src" "$dest_dir/$ORT_DYLIB_NAME"
}

apple_after_slice() {
  local slice_dir="$1" sbs_prefix="$2"
  local src
  src="$(ort_dylib_in "$sbs_prefix" || true)"
  stage_ort "$src" "$slice_dir"
  echo "[+]  $slice_dir/$ORT_DYLIB_NAME"
}

apple_after_macos_lipo() {
  local fat_dir="$2"
  lipo -create \
    "./dist/_work/macos-arm64/$ORT_DYLIB_NAME" \
    "./dist/_work/macos-amd64/$ORT_DYLIB_NAME" \
    -output "$fat_dir/$ORT_DYLIB_NAME"
}

apple_after_iossim_copy() {
  local sim_dir="$1"
  cp "./dist/_work/iossim-arm64/$ORT_DYLIB_NAME" "$sim_dir/$ORT_DYLIB_NAME"
}

apple_after_pack() {
  local root work ort_xcfw
  root="$(pwd)"
  work="$root/dist/_work"
  ort_xcfw="$root/dist/onnxruntime.xcframework"
  rm -rf "$ort_xcfw"
  echo ""
  # simplybs ships ORT without DWARF. App Store still requires a dSYM whose
  apple_xcframework_from_dylibs "$ort_xcfw" onnxruntime \
    "$work/ios-arm64/$ORT_DYLIB_NAME" - \
    "$work/iossim-fat/$ORT_DYLIB_NAME" - \
    "$work/macos-fat/$ORT_DYLIB_NAME" -
  echo ""
  echo "[+] Done: $ort_xcfw ($(du -sh "$ort_xcfw" | awk '{print $1}'))"
  find "$ort_xcfw" -type d -name "*.framework" | sort | sed 's/^/     /'
}

rm -rf ./dist/onnxruntime.xcframework
apple_cmake_xcframework eyes 14.0 11.0 include/eyes.h -DBUILD_TEST_EYES=OFF