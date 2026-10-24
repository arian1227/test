#!/usr/bin/env bash
# Builds libkmod.so (arm64) and classes2.dex (zzmod.ModMenu). Works on GitHub Actions or any PC with Android SDK + NDK.
set -euo pipefail
cd "$(dirname "$0")"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
NDK="${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}}"
[ -d "$SDK" ] || { echo "ANDROID_HOME / ANDROID_SDK_ROOT not set"; exit 1; }
[ -d "$NDK" ] || { echo "Android NDK not found (set ANDROID_NDK_HOME)"; exit 1; }
JAR=$(ls -d "$SDK"/platforms/android-*/android.jar | sort -V | tail -1)
BT=$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)
CC=$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/aarch64-linux-android23-clang | head -1)
echo "android.jar: $JAR"; echo "build-tools: $BT"; echo "cc: $CC"

rm -rf out build
mkdir -p out/lib/arm64-v8a build/classes

# native hooks
"$CC" -shared -fPIC -O2 -s -fno-stack-protector -Wl,-z,max-page-size=16384 \
  -o out/lib/arm64-v8a/libkmod.so kmod.c -ldl

# java menu -> dex
javac --release 8 -cp "$JAR" -d build/classes ModMenu.java
"$BT/d8" --release --min-api 23 --lib "$JAR" --output build $(find build/classes -name '*.class')
cp build/classes.dex out/classes2.dex

ls -l out out/lib/arm64-v8a
