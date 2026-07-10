#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/_lib.sh"

venv_activate
pip_ensure nuitka "-U"

OUTDIR="$ROOT/build"
LOCALE_JSON="$ROOT/Locale/locale.json"
LOCALE_BAK="$ROOT/locale.json.bak"
PROJ="$ROOT/Sample/Main.proj"
PROJ_BAK="$ROOT/Main.proj.bak"
PLATFORM="$("$PYTHON" -c 'import sys; print(sys.platform)')"
PROJ_REPLACED=0

[ -f "$LOCALE_JSON" ] || err "Locale JSON file not found: $LOCALE_JSON"

cleanup() {
    local rc=$?
    step "Cleaning up packaging state..."
    if [ -f "$LOCALE_BAK" ]; then
        [ -f "$LOCALE_JSON" ] && rm -f "$LOCALE_JSON"
        mv "$LOCALE_BAK" "$LOCALE_JSON"
    fi
    if [ "$PROJ_REPLACED" = "1" ] && [ -f "$PROJ" ]; then
        rm -f "$PROJ"
    fi
    if [ -f "$PROJ_BAK" ]; then
        mv "$PROJ_BAK" "$PROJ"
    fi
    exit "$rc"
}
trap cleanup EXIT

step "Generating locale pickles..."
"$PYTHON" "$ROOT/tools/localeTransfer.py" "$LOCALE_JSON"

if [ -f "$LOCALE_JSON" ]; then
    mv "$LOCALE_JSON" "$LOCALE_BAK"
fi
if [ -f "$PROJ" ]; then
    mv "$PROJ" "$PROJ_BAK"
fi
printf '{}' > "$PROJ"
PROJ_REPLACED=1

NUITKA_FLAGS=(
    --remove-output
    "--output-dir=$OUTDIR"
    "--company-name=Metempsychosis Game Studio"
    "--product-name=Ludork"
    "--file-version=1.0.0.0"
    "--product-version=1.0.0.0"
    "--file-description=Ludork Editor"
    "--copyright=Copyright (c) 2026 Ludork"
    --enable-plugin=pyqt5
    --include-qt-plugins=platforms,styles,iconengines,imageformats,qml
    --include-package-data=qt_material
    --include-module=EditorGlobal
    --include-package=Widgets
    --include-package=Utils
    "--include-data-dir=$ROOT/Resource=Resource"
    "--include-data-dir=$ROOT/Locale=Locale"
    "--include-data-dir=$ROOT/Styles=Styles"
    "--include-data-dir=$ROOT/EditorGlobal/Qml=EditorGlobal/Qml"
    "--include-data-dir=$ROOT/BuildTools=BuildTools"
    --include-module=NodeGraphQt
    --include-package=debugpy
    --include-module=asyncio
    --include-module=psutil
    --include-module=pympler.asizeof
    --include-module=av
    --include-module=openpyxl
    --include-package=agent
    "--include-data-dir=$ROOT/agent=agent"
    --include-module=quickjs
    --include-module=_quickjs
    --include-package=openai
    --include-module=PyQt5.QtSvg
    --nofollow-import-to=Engine
    --nofollow-import-to=Global
    --nofollow-import-to=Source
    --lto=yes
)

if [ "$PLATFORM" = "win32" ]; then
    [ -f "$ROOT/Resource/icon.ico" ] && NUITKA_FLAGS+=("--windows-icon-from-ico=$ROOT/Resource/icon.ico")
    NUITKA_FLAGS+=(--windows-console-mode=disable --standalone)
elif [ "$PLATFORM" = "darwin" ]; then
    [ -f "$ROOT/Resource/icon.icns" ] && NUITKA_FLAGS+=("--macos-app-icon=$ROOT/Resource/icon.icns")
    NUITKA_FLAGS+=(--disable-ccache --mode=app --macos-app-name=Ludork --output-filename=Ludork)
else
    err "Unsupported OS: $PLATFORM"
fi

step "Running Nuitka build..."
(cd "$ROOT" && "$PYTHON" -m nuitka "${NUITKA_FLAGS[@]}" "$ROOT/main.py")

if [ "$PLATFORM" = "win32" ]; then
    RUNTIME="$OUTDIR/main.dist"
else
    RUNTIME="$OUTDIR/main.app/Contents/MacOS"
fi

copy_runtime_dir() {
    local name="$1"
    local src="$ROOT/$name"
    local dst="$RUNTIME/$name"

    if [ ! -d "$src" ]; then
        warn "Asset directory not found: $src"
        return
    fi

    rm -rf "$dst"
    mkdir -p "$dst"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --exclude='__pycache__' --exclude='*.pyc' --exclude='*.pyo' "$src/" "$dst/"
    else
        (cd "$src" && tar --exclude='__pycache__' --exclude='*.pyc' --exclude='*.pyo' -cf - .) | (cd "$dst" && tar -xf -)
    fi
}

step "Copying Sample directory..."
copy_runtime_dir "Sample"

step "Copying Plugins directory..."
copy_runtime_dir "Plugins"

if [ "$PLATFORM" = "darwin" ]; then
    for name in ios_python iOSTemplate; do
        if [ -d "$ROOT/$name" ]; then
            step "Copying $name..."
            copy_runtime_dir "$name"
        else
            warn "$name directory not found; related editor packaging features will be unavailable."
        fi
    done

    APP="$OUTDIR/Ludork.app"
    rm -rf "$APP"
    mv "$OUTDIR/main.app" "$APP"
    log "Packaging complete. Output: $APP"
else
    log "Packaging complete. Output: $RUNTIME"
fi
