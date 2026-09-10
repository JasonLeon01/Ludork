#!/usr/bin/env sh
set -eu

SOURCE_DIR=$1
BUILD_DIR=$2
INSTALL_DIR=$3
GNU_MAKE=$4
TOOLCHAIN_ROOT=$5
API=$6

TARGET=aarch64-linux-android
TOOLCHAIN_BIN="$TOOLCHAIN_ROOT/bin"
SYSROOT="$TOOLCHAIN_ROOT/sysroot"
CC="$TOOLCHAIN_BIN/clang --target=$TARGET$API"
CXX="$TOOLCHAIN_BIN/clang++ --target=$TARGET$API"
AR="$TOOLCHAIN_BIN/llvm-ar"
RANLIB="$TOOLCHAIN_BIN/llvm-ranlib"
NM="$TOOLCHAIN_BIN/llvm-nm"
STRIP="$TOOLCHAIN_BIN/llvm-strip"

for tool in clang clang++ llvm-ar llvm-ranlib llvm-nm llvm-strip; do
    if [ ! -x "$TOOLCHAIN_BIN/$tool" ]; then
        echo "Android NDK tool was not found: $TOOLCHAIN_BIN/$tool" >&2
        exit 1
    fi
done
if [ ! -d "$SYSROOT" ]; then
    echo "Android NDK sysroot was not found: $SYSROOT" >&2
    exit 1
fi

rm -rf "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"
cd "$BUILD_DIR"

HOST_SDK_PATH=$(xcrun --sdk macosx --show-sdk-path)
HOST_CC=$(xcrun --sdk macosx --find clang)
HOST_FLAGS="--sysroot=$HOST_SDK_PATH"

"$SOURCE_DIR/configure" \
    --prefix="$INSTALL_DIR" \
    --enable-cross-compile \
    --target-os=android \
    --arch=aarch64 \
    --cpu=armv8-a \
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
    --disable-jni \
    --disable-mediacodec \
    --disable-iconv \
    --enable-pthreads \
    --enable-small \
    --enable-pic \
    --disable-asm \
    --enable-protocol=file \
    --enable-demuxer=mov \
    --enable-decoder=h264,aac \
    --enable-parser=h264,aac \
    --enable-swscale \
    --enable-swresample \
    --host-cc="$HOST_CC" \
    --host-cflags="$HOST_FLAGS" \
    --host-ld="$HOST_CC" \
    --host-ldflags="$HOST_FLAGS"
"$GNU_MAKE" -j "$(sysctl -n hw.ncpu)" install

for library in avutil avcodec avformat swscale swresample; do
    if [ ! -f "$INSTALL_DIR/lib/lib$library.a" ]; then
        echo "FFmpeg Android static library was not produced: lib$library.a" >&2
        exit 1
    fi
done
