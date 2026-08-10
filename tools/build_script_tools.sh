#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
cd "$PROJECT_ROOT"

PYTHON="$PROJECT_ROOT/.venv/bin/python"
SOURCE_DIR="$PROJECT_ROOT/ScriptTools"
OUTPUT_DIR="$PROJECT_ROOT/.tools/ScriptTools"
OUTPUT="$OUTPUT_DIR/ScriptTools"
STAMP="$OUTPUT_DIR/source.sha256"
VERSION_REPORT="$OUTPUT_DIR/runtime-versions.txt"

if [ ! -x "$PYTHON" ]; then
    echo "Python environment was not found. Run tools/setup_python.sh first." >&2
    exit 1
fi
if [ ! -f "$SOURCE_DIR/__main__.py" ]; then
    echo "ScriptTools source was not found: $SOURCE_DIR" >&2
    exit 1
fi
if ! "$PYTHON" -c 'import sys; raise SystemExit(0 if sys.version_info[:2] == (3, 12) else 1)'; then
    echo "ScriptTools requires a Python 3.12 virtual environment. Run tools/setup_python.sh again." >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
PYTHON_VERSION=$("$PYTHON" -c "import platform; print(platform.python_version())")
NUITKA_VERSION=$("$PYTHON" -c "import importlib.metadata; print(importlib.metadata.version('Nuitka'))")
OPENSSL_VERSION=$("$PYTHON" -c "import ssl; print(ssl.OPENSSL_VERSION)")
PYTHON_ZSTANDARD_VERSION=$("$PYTHON" -c "import importlib.metadata; print(importlib.metadata.version('zstandard'))")
NUITKA_ZSTD_VERSION=1.4.7
PLATFORM_DESCRIPTION=$("$PYTHON" -c "import platform; print(f'{platform.system()}-{platform.machine()}')")
SOURCE_HASH=$(
    "$PYTHON" -c "import hashlib, pathlib, sys; root=pathlib.Path(sys.argv[1]); files=sorted(root.rglob('*.py')); digest=hashlib.sha256(); [digest.update(path.relative_to(root).as_posix().encode('utf-8') + b'\0' + path.read_bytes()) for path in files]; digest.update(sys.argv[2].encode('utf-8')); print(digest.hexdigest())" "$SOURCE_DIR" "$PLATFORM_DESCRIPTION|$PYTHON_VERSION|$NUITKA_VERSION|$OPENSSL_VERSION|$PYTHON_ZSTANDARD_VERSION|$NUITKA_ZSTD_VERSION"
)
{
    printf 'Platform: %s\n' "$PLATFORM_DESCRIPTION"
    printf 'CPython: %s\n' "$PYTHON_VERSION"
    printf 'OpenSSL: %s\n' "$OPENSSL_VERSION"
    printf 'Nuitka: %s\n' "$NUITKA_VERSION"
    printf 'Nuitka onefile Zstandard: %s\n' "$NUITKA_ZSTD_VERSION"
    printf 'python-zstandard build compressor: %s\n' "$PYTHON_ZSTANDARD_VERSION"
} > "$VERSION_REPORT"
PREVIOUS_HASH=
if [ -f "$STAMP" ]; then
    PREVIOUS_HASH=$(cat "$STAMP")
fi
if [ -x "$OUTPUT" ] && [ "$SOURCE_HASH" = "$PREVIOUS_HASH" ]; then
    echo "Using current ScriptTools: $OUTPUT"
    exit 0
fi

echo "Building standalone ScriptTools..."
"$PYTHON" -m nuitka \
    --mode=onefile \
    --assume-yes-for-downloads \
    --include-package=ScriptTools \
    --output-dir="$OUTPUT_DIR" \
    --output-filename=ScriptTools \
    "$SOURCE_DIR/__main__.py"
if [ ! -x "$OUTPUT" ]; then
    echo "Nuitka did not produce $OUTPUT" >&2
    exit 1
fi
printf '%s' "$SOURCE_HASH" > "$STAMP"
echo "ScriptTools ready: $OUTPUT"
