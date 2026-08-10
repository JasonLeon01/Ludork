#!/usr/bin/env sh
set -u

installer_directory=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
payload_root="$installer_directory/.official-plugin-payload"
script_tools="$installer_directory/Ludork.app/Contents/Resources/tools/ScriptTools"

show_error() {
    LUDORK_INSTALLER_MESSAGE=$1 /usr/bin/osascript \
        -e 'display alert "Official plug-in installation failed" message (system attribute "LUDORK_INSTALLER_MESSAGE") as critical buttons {"OK"} default button "OK"' \
        >/dev/null
}

require_ludork_closed() {
    if /usr/bin/pgrep -x Ludork >/dev/null 2>&1; then
        show_error "Close Ludork before installing the official plug-ins."
        return 1
    fi
    return 0
}

if [ -z "${HOME:-}" ] || [ ! -d "$HOME" ]; then
    show_error "The current user home directory is unavailable."
    exit 1
fi

if ! require_ludork_closed; then
    exit 1
fi

if [ ! -x "$script_tools" ]; then
    show_error "The installer tool is missing from Ludork.app."
    exit 1
fi

if ! validation_output=$(
    "$script_tools" editor-official-plugins validate "$payload_root" 2>&1
); then
    validation_message=$(printf 'The DMG plug-in payload is invalid.\n\n%.1600s' "$validation_output")
    show_error "$validation_message"
    exit 1
fi

if ! confirmation=$(/usr/bin/osascript 2>/dev/null <<'APPLESCRIPT'
button returned of (display dialog "This replaces ~/Ludork/Plugins and ~/Ludork/plugins.json. Existing third-party plug-ins, registrations and Plugins/.data will be removed. No backup is created." with title "Install Official Plug-ins" buttons {"Cancel", "Replace and Install"} default button "Cancel" cancel button "Cancel" with icon caution)
APPLESCRIPT
); then
    exit 0
fi

if [ "$confirmation" != "Replace and Install" ]; then
    exit 0
fi

if ! require_ludork_closed; then
    exit 1
fi

target_root="$HOME/Ludork"
if ! install_output=$(
    "$script_tools" editor-official-plugins prepare \
        "$payload_root/Plugins" \
        "$target_root" 2>&1
); then
    install_message=$(printf 'Installation failed; review the recovery details below.\n\n%.1600s' "$install_output")
    show_error "$install_message"
    exit 1
fi

if ! validation_output=$(
    "$script_tools" editor-official-plugins validate "$target_root" 2>&1
); then
    validation_message=$(printf 'Installed files did not pass validation.\n\n%.1600s' "$validation_output")
    show_error "$validation_message"
    exit 1
fi

/usr/bin/osascript \
    -e 'display dialog "Official plug-ins were installed in ~/Ludork." with title "Installation complete" buttons {"OK"} default button "OK" with icon note' \
    >/dev/null
if ! /usr/bin/open "$target_root" >/dev/null 2>&1; then
    :
fi
exit 0
