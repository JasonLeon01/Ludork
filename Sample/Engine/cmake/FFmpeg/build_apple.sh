#!/usr/bin/env sh
set -eu

SOURCE_DIR=$1
BUILD_DIR=$2
INSTALL_DIR=$3
GNU_MAKE=$4
PLATFORM=$5
DEPLOYMENT_TARGET=$6

rm -rf "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"
cd "$BUILD_DIR"

HOST_SDK_PATH=$(xcrun --sdk macosx --show-sdk-path)
HOST_CC=$(xcrun --sdk macosx --find clang)
HOST_FLAGS="--sysroot=$HOST_SDK_PATH"

if [ "$PLATFORM" = "ios" ]; then
    SDK_PATH=$(xcrun --sdk iphoneos --show-sdk-path)
    CC=$(xcrun --sdk iphoneos --find clang)
    PLATFORM_ARGS="--enable-cross-compile --target-os=darwin --arch=arm64 --cc=$CC --sysroot=$SDK_PATH --disable-asm"
    DEPLOYMENT_FLAGS="-miphoneos-version-min=$DEPLOYMENT_TARGET"
    LINKAGE_ARGS="--enable-static --disable-shared"
else
    SDK_PATH=$(xcrun --sdk macosx --show-sdk-path)
    CC=$(xcrun --sdk macosx --find clang)
    PLATFORM_ARGS="--target-os=darwin --arch=arm64 --cc=$CC --sysroot=$SDK_PATH --disable-asm"
    DEPLOYMENT_FLAGS="-mmacosx-version-min=$DEPLOYMENT_TARGET"
    LINKAGE_ARGS="--install-name-dir=@rpath --enable-shared --disable-static"
fi

"$SOURCE_DIR/configure" \
    --prefix="$INSTALL_DIR" \
    $PLATFORM_ARGS \
    $LINKAGE_ARGS \
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
    --enable-protocol=file \
    --enable-demuxer=mov \
    --enable-decoder=h264,aac \
    --enable-parser=h264,aac \
    --enable-swscale \
    --enable-swresample \
    --host-cc="$HOST_CC" \
    --host-cflags="$HOST_FLAGS" \
    --host-ld="$HOST_CC" \
    --host-ldflags="$HOST_FLAGS" \
    --extra-cflags="$DEPLOYMENT_FLAGS" \
    --extra-ldflags="$DEPLOYMENT_FLAGS"
"$GNU_MAKE" -j "$(sysctl -n hw.ncpu)" install
