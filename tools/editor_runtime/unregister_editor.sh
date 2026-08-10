#!/usr/bin/env sh
set -eu

TOOLS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$#" -gt 1 ]; then
    echo "Usage: tools/unregister_editor.sh [Ludork.app]" >&2
    exit 1
fi

if [ "$#" -eq 1 ]; then
    if [ ! -d "$1" ]; then
        echo "Ludork.app was not found: $1" >&2
        exit 1
    fi
    APP_DIR=$(CDPATH= cd -- "$1" && pwd)
else
    case "$TOOLS_DIR" in
        */Contents/Resources/tools)
            APP_DIR=$(CDPATH= cd -- "$TOOLS_DIR/../../.." && pwd)
            ;;
        *)
            echo "Pass the path to Ludork.app when running outside the packaged editor." >&2
            exit 1
            ;;
    esac
fi

INFO_PLIST="$APP_DIR/Contents/Info.plist"
if [ ! -f "$INFO_PLIST" ]; then
    echo "Info.plist was not found: $INFO_PLIST" >&2
    exit 1
fi
if [ "$(plutil -extract CFBundleIdentifier raw -o - "$INFO_PLIST")" != "com.ludork.editor" ]; then
    echo "The selected app is not the Ludork editor." >&2
    exit 1
fi

LSREGISTER=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
if [ ! -x "$LSREGISTER" ]; then
    echo "LaunchServices registration tool was not found." >&2
    exit 1
fi

"$LSREGISTER" -u "$APP_DIR"
echo "Ludork project association unregistered: $APP_DIR"
