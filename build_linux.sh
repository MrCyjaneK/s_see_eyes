#!/usr/bin/env bash
# Build libeyes.so + libonnxruntime.so for Linux (amd64, arm64).
#
# Prereqs:
#   make simplybs-x86_64-linux-gnu simplybs-aarch64-linux-gnu
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
# shellcheck source=../linux-gnu.sh
source "$SCRIPT_DIR/../linux-gnu.sh"

LIB_NAME=eyes
SO_NAME=lib${LIB_NAME}.so
OUT_DIR=./dist/linux
WORK=./dist/_work-linux
ORT_SO_NAME=libonnxruntime.so
ORT_PROVIDERS_SO=libonnxruntime_providers_shared.so

require() { command -v "$1" &>/dev/null || { echo "[!] '$1' not found"; exit 1; }; }
require cmake
require python3

ort_so_in() {
  local prefix="$1"
  local f
  for f in "$prefix"/lib/libonnxruntime.so.*; do
    [[ -f "$f" && ! -L "$f" ]] || continue
    echo "$f"
    return 0
  done
  return 1
}

require_sbs() {
  local prefix="$1"
  if ! ort_so_in "$prefix" >/dev/null; then
    echo "[!] missing libonnxruntime.so.* in $prefix/lib — run: make simplybs" >&2
    exit 1
  fi
}

stage_ort() {
  local src="$1" dest_dir="$2"
  mkdir -p "$dest_dir"
  cp -f "$src" "$dest_dir/$ORT_SO_NAME"
  chmod u+w "$dest_dir/$ORT_SO_NAME"
  linux_set_soname "$dest_dir/$ORT_SO_NAME" "$ORT_SO_NAME"
}

stage_prefix() {
  local sbs_prefix="$1" slice="$2"
  local staged="$slice/prefix"
  mkdir -p "$staged/lib"
  ln -sfn "$sbs_prefix/include" "$staged/include"
  ln -sfn "$sbs_prefix/lib/libZXing.a" "$staged/lib/libZXing.a"
  ln -sfn "$sbs_prefix/lib/libeasyexif.a" "$staged/lib/libeasyexif.a"
  local ort_src
  ort_src="$(ort_so_in "$sbs_prefix")"
  stage_ort "$ort_src" "$staged/lib"
  if [[ -f "$sbs_prefix/lib/$ORT_PROVIDERS_SO" ]]; then
    cp -f "$sbs_prefix/lib/$ORT_PROVIDERS_SO" "$staged/lib/$ORT_PROVIDERS_SO"
  fi
  echo "$staged"
}

cmake_slice() {
  local arch="$1" sbs_triplet="$2" cc_prefix="$3"
  local sbs_prefix="$SBS_ROOT/$sbs_triplet"
  require_sbs "$sbs_prefix"

  local cc cxx
  cc="$(linux_cc "$cc_prefix")"
  cxx="$(linux_cxx "$cc_prefix")"
  if ! command -v "$cc" &>/dev/null; then
    echo "[!] missing $cc" >&2
    exit 1
  fi

  local slice="$WORK/$arch"
  local build_dir="build-linux-$arch"
  rm -rf "$build_dir" "$slice"
  mkdir -p "$slice" "$OUT_DIR/$arch"

  echo "[+] CMake $arch (sbs=$sbs_triplet)..."
  local staged
  staged="$(stage_prefix "$sbs_prefix" "$slice")"

  cmake -S . -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$cc" \
    -DCMAKE_CXX_COMPILER="$cxx" \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g" \
    -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -g" \
    -DSBS_PREFIX="$staged" \
    -DBUILD_TEST_EYES=OFF

  cmake --build "$build_dir" -j"$(linux_jobs)"

  cp -f "$build_dir/$SO_NAME" "$OUT_DIR/$arch/"
  cp -f "$staged/lib/$ORT_SO_NAME" "$OUT_DIR/$arch/"
  if [[ -f "$staged/lib/$ORT_PROVIDERS_SO" ]]; then
    cp -f "$staged/lib/$ORT_PROVIDERS_SO" "$OUT_DIR/$arch/"
  fi
  echo "[+]  $OUT_DIR/$arch/$SO_NAME (+ $ORT_SO_NAME)"
}

rm -rf "$WORK" "$OUT_DIR"
mkdir -p "$WORK" "$OUT_DIR"

arch="" sbs_triplet="" goarch="" cc_prefix=""
host_goarch="$(go env GOARCH)"
while read -r arch sbs_triplet goarch cc_prefix; do
  [[ -n "$arch" ]] || continue
  linux_want_sbs_slice "$arch" "$sbs_triplet" "$goarch" "$host_goarch" || continue
  cmake_slice "$arch" "$sbs_triplet" "$cc_prefix"
done < <(linux_arch_rows)

rm -rf "$WORK" build-linux-*

host_arch=""
case "$(uname -m)" in
  x86_64) host_arch=amd64 ;;
  aarch64|arm64) host_arch=arm64 ;;
esac
if [[ -n "$host_arch" && -f "$OUT_DIR/$host_arch/$SO_NAME" ]]; then
  mkdir -p dist
  cp -f "$OUT_DIR/$host_arch/$SO_NAME" dist/
  echo "[+]  dist/$SO_NAME (host)"
fi

echo ""
echo "[+] Done: $OUT_DIR"
find "$OUT_DIR" -name "*.so" | sort | sed 's/^/     /'
echo ""
echo "[i] Ship ONNX assets separately; host passes assets directory to SPACESEE_eyes_load."
echo ""
