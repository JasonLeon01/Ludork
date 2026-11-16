#!/usr/bin/env bash
set -eu

SOURCE_DIR=$1
BUILD_DIR=$2
INSTALL_DIR=$3
GNU_MAKE=$4
EMSCRIPTEN_DIR=$5
HOST_CC=$6

if command -v cygpath >/dev/null 2>&1; then
    SOURCE_DIR=$(cygpath -u "$SOURCE_DIR")
    BUILD_DIR=$(cygpath -u "$BUILD_DIR")
    INSTALL_DIR=$(cygpath -u "$INSTALL_DIR")
    GNU_MAKE=$(cygpath -u "$GNU_MAKE")
    EMSCRIPTEN_DIR=$(cygpath -u "$EMSCRIPTEN_DIR")
    HOST_CC=$(cygpath -u "$HOST_CC")
fi

mkdir -p "$BUILD_DIR" "$INSTALL_DIR"
cd "$BUILD_DIR"
BUILD_DIR=$(pwd)
INSTALL_DIR=$(cd "$INSTALL_DIR" && pwd)
if [ "$INSTALL_DIR" != "$(dirname "$BUILD_DIR")/install" ]; then
    echo "The FFmpeg install directory must be the build directory's sibling named install." >&2
    exit 1
fi
mkdir -p src
if [ -L src ]; then
    echo "The FFmpeg work source must not be a symbolic link." >&2
    exit 1
fi
cp -R "$SOURCE_DIR/." src/
HOST_CC_DIRECTORY=$(dirname "$HOST_CC")
HOST_CC=$(basename "$HOST_CC")
LLVM_BIN="$EMSCRIPTEN_DIR/../bin"
SHELL_DIRECTORY=$(dirname "$(command -v bash)")
export PATH="$HOST_CC_DIRECTORY:$EMSCRIPTEN_DIR:$LLVM_BIN:$SHELL_DIRECTORY:$PATH"

bash src/configure \
    --prefix=../install \
    --enable-cross-compile --target-os=none --arch=wasm32 \
    --cc=emcc --cxx=em++ --ld=emcc --ar=emar --ranlib=emranlib --nm=llvm-nm \
    --host-cc="$HOST_CC" --host-ld="$HOST_CC" \
    --enable-static --disable-shared \
    --disable-gpl --disable-version3 --disable-nonfree \
    --disable-everything --disable-autodetect \
    --disable-programs --disable-doc --disable-debug \
    --disable-network --disable-avdevice --disable-avfilter --disable-hwaccels \
    --disable-pthreads --disable-w32threads --disable-os2threads \
    --disable-asm --disable-runtime-cpudetect --disable-symver --disable-stripping \
    --disable-hardcoded-tables --disable-iconv \
    --enable-small --enable-protocol=file --enable-demuxer=mov \
    --enable-decoder=h264,aac --enable-parser=h264,aac \
    --enable-swscale --enable-swresample --extra-cflags=-O2

MAKE_SHELL=$(command -v bash)
if command -v cygpath >/dev/null 2>&1; then
    MAKE_SHELL=$(cygpath -m "$MAKE_SHELL")
fi
"$GNU_MAKE" -f src/Makefile -j "${LUDORK_FFMPEG_BUILD_JOBS:-${NUMBER_OF_PROCESSORS:-4}}" \
    "SHELL=$MAKE_SHELL" "REVISION=$(cat src/RELEASE)" CCDEP= CXXDEP= ASDEP= HOSTCCDEP= install
for library in avutil avcodec avformat swscale swresample; do
    if [ ! -s "$INSTALL_DIR/lib/lib$library.a" ]; then
        echo "FFmpeg Web static library was not produced: lib$library.a" >&2
        exit 1
    fi
done
