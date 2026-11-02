#!/usr/bin/env bash
#
# Build the library for every target this machine can reach.
#
#   bash scripts/build-all.sh
#
# Output lands in dist/, named with the version, ready to attach to a release.
# Targets whose toolchain is missing are skipped with a note instead of failing
# the whole run, so a Linux box still produces Linux and Android binaries.

set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=$(sed -n 's/^version = "\(.*\)"/\1/p' Cargo.toml | head -1)
DIST="dist"
mkdir -p "$DIST"

say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }
target_installed() { rustup target list --installed 2>/dev/null | grep -qx "$1"; }

say "host build ($VERSION)"
cargo build --release

case "$(uname -s)" in
  Darwin)
    cp target/release/libhrt_core.dylib "$DIST/libhrt_core.dylib"
    cp target/release/libhrt_core.dylib "$DIST/hrt-core-$VERSION-macos-$(uname -m).dylib"
    ;;
  Linux)
    cp target/release/libhrt_core.so "$DIST/libhrt_core.so"
    cp target/release/libhrt_core.so "$DIST/hrt-core-$VERSION-linux-$(uname -m).so"
    ;;
  MINGW* | MSYS* | CYGWIN*)
    cp target/release/hrt_core.dll "$DIST/hrt_core.dll"
    cp target/release/hrt_core.dll "$DIST/hrt-core-$VERSION-windows-x64.dll"
    ;;
esac
cp include/hrt_core.h "$DIST/hrt_core.h"

if [ -n "${ANDROID_NDK_HOME:-}" ] && have cargo-ndk; then
  say "android (arm64-v8a, armeabi-v7a, x86_64)"
  cargo ndk -t arm64-v8a -t armeabi-v7a -t x86_64 \
    -o "$DIST/android" build --release
  for abi in arm64-v8a armeabi-v7a x86_64; do
    cp "$DIST/android/$abi/libhrt_core.so" \
       "$DIST/hrt-core-$VERSION-android-$abi.so"
  done
else
  say "android skipped (set ANDROID_NDK_HOME and install cargo-ndk)"
fi

if [ "$(uname -s)" = "Darwin" ] && have lipo; then
  say "ios"
  rustup target add aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios 2>/dev/null || true
  for t in aarch64-apple-ios aarch64-apple-ios-sim x86_64-apple-ios; do
    if target_installed "$t"; then
      cargo build --release --target "$t"
    else
      echo "  $t not installed, skipped"
    fi
  done
  if [ -f target/aarch64-apple-ios/release/libhrt_core.a ]; then
    cp target/aarch64-apple-ios/release/libhrt_core.a \
       "$DIST/hrt-core-$VERSION-ios-device.a"
  fi
  SIM_LIBS=()
  [ -f target/aarch64-apple-ios-sim/release/libhrt_core.a ] && \
    SIM_LIBS+=("target/aarch64-apple-ios-sim/release/libhrt_core.a")
  [ -f target/x86_64-apple-ios/release/libhrt_core.a ] && \
    SIM_LIBS+=("target/x86_64-apple-ios/release/libhrt_core.a")
  if [ ${#SIM_LIBS[@]} -gt 0 ]; then
    lipo -create "${SIM_LIBS[@]}" -output \
      "$DIST/hrt-core-$VERSION-ios-simulator.a"
  fi

  say "macos universal"
  rustup target add x86_64-apple-darwin aarch64-apple-darwin 2>/dev/null || true
  if target_installed x86_64-apple-darwin && target_installed aarch64-apple-darwin; then
    cargo build --release --target x86_64-apple-darwin
    cargo build --release --target aarch64-apple-darwin
    lipo -create \
      target/x86_64-apple-darwin/release/libhrt_core.dylib \
      target/aarch64-apple-darwin/release/libhrt_core.dylib \
      -output "$DIST/hrt-core-$VERSION-macos-universal.dylib"
  fi
else
  say "ios/macos universal skipped (needs macOS)"
fi

say "done"
ls -lh "$DIST"
