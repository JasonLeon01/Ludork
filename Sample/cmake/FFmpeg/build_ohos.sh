#!/usr/bin/env sh
set -eu

SOURCE_DIR=$1
BUILD_DIR=$2
INSTALL_DIR=$3
GNU_MAKE=$4
OHOS_SDK_NATIVE=$5

TOOLCHAIN_BIN="$OHOS_SDK_NATIVE/llvm/bin"
SYSROOT="$OHOS_SDK_NATIVE/sysroot"
CC="$TOOLCHAIN_BIN/aarch64-unknown-linux-ohos-clang"
CXX="$TOOLCHAIN_BIN/aarch64-unknown-linux-ohos-clang++"
AR="$TOOLCHAIN_BIN/llvm-ar"
RANLIB="$TOOLCHAIN_BIN/llvm-ranlib"
NM="$TOOLCHAIN_BIN/llvm-nm"
STRIP="$TOOLCHAIN_BIN/llvm-strip"

rm -rf "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"
cd "$BUILD_DIR"

"$SOURCE_DIR/configure" \
    --prefix="$INSTALL_DIR" \
    --enable-cross-compile \
    --target-os=linux \
    --arch=aarch64 \
    --cc="$CC" \
    --cxx="$CXX" \
    --ar="$AR" \
    --ranlib="$RANLIB" \
    --nm="$NM" \
    --strip="$STRIP" \
    --sysroot="$SYSROOT" \
    --enable-static \
    --disable-shared \
    --disable-gpl \
    --disable-version3 \
    --disable-nonfree \
    --disable-everything \
    --disable-autodetect \
    --disable-programs \
    --disable-doc \
    --disable-network \
    --disable-avdevice \
    --disable-avfilter \
    --disable-hwaccels \
    --disable-runtime-cpudetect \
    --disable-debug \
    --enable-small \
    --enable-pic \
    --disable-asm \
    --enable-protocol=file \
    --enable-demuxer=mov \
    --enable-decoder=h264,aac \
    --enable-parser=h264,aac \
    --enable-swscale \
    --enable-swresample \
    --extra-cflags=-D__MUSL__ \
    --extra-ldflags=-Wl,--build-id=sha1
"$GNU_MAKE" -j "$(sysctl -n hw.ncpu)" install
