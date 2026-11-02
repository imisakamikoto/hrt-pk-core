#!/usr/bin/env bash
#
# Everything CI runs, plus the two binding self-tests. Run this before tagging.
#
#   bash scripts/verify.sh

set -euo pipefail
cd "$(dirname "$0")/.."

say() { printf '\n\033[1m==> %s\033[0m\n' "$*"; }

say "format"
cargo fmt --check

say "tests"
cargo test --release

say "release build"
cargo build --release

say "header compiles and the ABI answers"
if command -v cc >/dev/null 2>&1; then
  case "$(uname -s)" in
    Darwin) LIB="target/release/libhrt_core.dylib" ;;
    Linux) LIB="target/release/libhrt_core.so" ;;
    *) LIB="" ;;
  esac
  if [ -n "$LIB" ]; then
    cc -Wall -Wextra -Iinclude examples/c/example.c \
       -Ltarget/release -lhrt_core -Wl,-rpath,"$PWD/target/release" \
       -o /tmp/hrt_core_example
    /tmp/hrt_core_example
  fi
else
  echo "cc not found, skipping the C example"
fi

say "python binding"
if command -v python3 >/dev/null 2>&1; then
  case "$(uname -s)" in
    Darwin) export HRT_CORE_LIB="$PWD/target/release/libhrt_core.dylib" ;;
    Linux) export HRT_CORE_LIB="$PWD/target/release/libhrt_core.so" ;;
  esac
  python3 bindings/python/hrt_core.py
fi

say "exported symbols"
LIB=$(ls target/release/libhrt_core.{dylib,so} 2>/dev/null | head -1 || true)
if [ -n "$LIB" ] && command -v nm >/dev/null 2>&1; then
  nm -gU "$LIB" 2>/dev/null | grep -o 'hrt_[a-z_]*' | sort -u || \
    nm -D "$LIB" | grep -o 'hrt_[a-z_]*' | sort -u
fi

say "ok"
