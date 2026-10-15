#!/usr/bin/env bash
# Build libeyes.so + libonnxruntime.so for Android ABIs (arm64-v8a, armeabi-v7a, x86_64).
#
# Prereqs:
#   make simplybs-aarch64-linux-android simplybs-x86_64-linux-android simplybs-armv7a-linux-androideabi
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
# shellcheck source=../android-ndk.sh
source "$SCRIPT_DIR/../android-ndk.sh"

LIB_NAME=eyes
SO_NAME=lib${LIB_NAME}.so
OUT_DIR=./dist/android
WORK=./dist/_work-android
ORT_SO_NAME=libonnxruntime.so
ORT_PROVIDERS_SO=libonnxruntime_providers_shared.so

require() { command -v "$1" &>/dev/null || { echo "[!] '$1' not found"; exit 1; }; }
require cmake
require python3
android_require_ndk

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

# Android jniLibs only ships unversioned lib*.so; simplybs ORT uses SONAME libonnxruntime.so.1.
stage_ort() {
  local src="$1" dest_dir="$2"
  mkdir -p "$dest_dir"
  cp -f "$src" "$dest_dir/$ORT_SO_NAME"
  chmod u+w "$dest_dir/$ORT_SO_NAME"
  android_set_soname "$dest_dir/$ORT_SO_NAME" "$ORT_SO_NAME"
}

stage_prefix() {
  local sbs_prefix="$1" slice="$2" abi="$3"
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
  android_copy_libcxx "$(android_libcxx_triple "$abi")" "$staged/lib"
  echo "$staged"
}

cmake_slice() {
  local abi="$1" sbs_triplet="$2"
  local sbs_prefix="$SBS_ROOT/$sbs_triplet"

  local slice="$WORK/$abi"
  local build_dir="build-android-$abi"
  rm -rf "$build_dir" "$slice"
  mkdir -p "$slice" "$OUT_DIR/$abi"

  echo "[+] CMake $abi (sbs=$sbs_triplet)..."
  local staged
  staged="$(stage_prefix "$sbs_prefix" "$slice" "$abi")"

  cmake -S . -B "$build_dir" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" \
    -DANDROID_PLATFORM="android-$ANDROID_API" \
    -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -g" \
    -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG -g" \
    -DSBS_PREFIX="$staged" \
    -DBUILD_TEST_EYES=OFF

  cmake --build "$build_dir" -j"$(android_jobs)"

  cp -f "$build_dir/$SO_NAME" "$OUT_DIR/$abi/"
  cp -f "$staged/lib/$ORT_SO_NAME" "$OUT_DIR/$abi/"
  cp -f "$staged/lib/libc++_shared.so" "$OUT_DIR/$abi/"
  if [[ -f "$staged/lib/$ORT_PROVIDERS_SO" ]]; then
    cp -f "$staged/lib/$ORT_PROVIDERS_SO" "$OUT_DIR/$abi/"
  fi
  echo "[+]  $OUT_DIR/$abi/$SO_NAME (+ $ORT_SO_NAME)"
}

ROOT="$(pwd)"
rm -rf "$WORK" "$OUT_DIR"
mkdir -p "$WORK" "$OUT_DIR"

local_abi="" sbs_triplet="" goarch="" clang_prefix=""
while read -r local_abi sbs_triplet goarch clang_prefix; do
  [[ -n "$local_abi" ]] || continue
  cmake_slice "$local_abi" "$sbs_triplet"
done < <(android_abi_rows)

rm -rf "$WORK" build-android-*

echo ""
echo "[+] Done: $OUT_DIR"
find "$OUT_DIR" -name "*.so" | sort | sed 's/^/     /'
echo ""
echo "[i] SpaceDroid jniLibs.srcDirs points at this tree (and mock/archive dist/android)."
echo "[i] Ship ONNX assets separately; host passes assets directory to SPACESEE_eyes_load."
echo ""
