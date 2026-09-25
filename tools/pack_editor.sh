#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/common.sh"
. "$PROJECT_ROOT/versions.conf"
: "${FFMPEG_VERSION:?FFMPEG_VERSION is not set in versions.conf}"
: "${EDITOR_VERSION:?EDITOR_VERSION is not set in versions.conf}"

USAGE="Usage: tools/pack_editor.sh [--version X.Y.Z] [--dev|--release] [--templates <folder>|--without-templates] [--use-current-editor-build] [--signing-identity NAME] [--certificate PATH.p12] [--notarize] [--notary-apple-id EMAIL] [--notary-team-id TEAMID] [--notary-key PATH.p8] [--notary-key-id ID] [--notary-key-issuer UUID]"
PREBUILT_TEMPLATES_DIR=
WITHOUT_TEMPLATES=0
USE_CURRENT_EDITOR_BUILD=0
PRODUCT_VERSION=$EDITOR_VERSION
PACKAGE_CHANNEL=
SIGNING_IDENTITY=
SIGNING_CERTIFICATE=
SIGNING_ENTITLEMENTS=
SIGNING_NOTARIZE=0
SIGNING_NOTARY_APPLE_ID=
SIGNING_NOTARY_TEAM_ID=
SIGNING_NOTARY_KEY=
SIGNING_NOTARY_KEY_ID=
SIGNING_NOTARY_KEY_ISSUER=
while [ "$#" -gt 0 ]; do
    case "$1" in
        --version)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            PRODUCT_VERSION=$2
            shift 2
            ;;
        --dev|--release)
            if [ -n "$PACKAGE_CHANNEL" ] && [ "$PACKAGE_CHANNEL" != "$1" ]; then
                echo "--dev and --release are mutually exclusive." >&2
                exit 1
            fi
            PACKAGE_CHANNEL=$1
            shift
            ;;
        --templates)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            PREBUILT_TEMPLATES_DIR=$2
            shift 2
            ;;
        --without-templates)
            WITHOUT_TEMPLATES=1
            shift
            ;;
        --use-current-editor-build)
            USE_CURRENT_EDITOR_BUILD=1
            shift
            ;;
        --signing-identity)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_IDENTITY=$2
            shift 2
            ;;
        --certificate)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_CERTIFICATE=$2
            shift 2
            ;;
        --notarize)
            SIGNING_NOTARIZE=1
            shift
            ;;
        --notary-apple-id)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_NOTARY_APPLE_ID=$2
            shift 2
            ;;
        --notary-team-id)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_NOTARY_TEAM_ID=$2
            shift 2
            ;;
        --notary-key)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_NOTARY_KEY=$2
            shift 2
            ;;
        --notary-key-id)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_NOTARY_KEY_ID=$2
            shift 2
            ;;
        --notary-key-issuer)
            if [ "$#" -lt 2 ] || [ -z "$2" ]; then
                echo "$USAGE" >&2
                exit 1
            fi
            SIGNING_NOTARY_KEY_ISSUER=$2
            shift 2
            ;;
        *)
            echo "$USAGE" >&2
            exit 1
            ;;
    esac
done
if [ "$WITHOUT_TEMPLATES" -eq 1 ] && [ -n "$PREBUILT_TEMPLATES_DIR" ]; then
    echo "--templates and --without-templates are mutually exclusive." >&2
    exit 1
fi
if [ -z "$PACKAGE_CHANNEL" ]; then
    PACKAGE_CHANNEL=--dev
fi

SIGNING_IDENTITY=${LUDORK_MACOS_SIGNING_IDENTITY:-$SIGNING_IDENTITY}
SIGNING_CERTIFICATE=${LUDORK_MACOS_SIGNING_CERTIFICATE:-$SIGNING_CERTIFICATE}
SIGNING_ENTITLEMENTS=${LUDORK_MACOS_SIGNING_ENTITLEMENTS:-$SIGNING_ENTITLEMENTS}
SIGNING_NOTARY_APPLE_ID=${LUDORK_MACOS_NOTARY_APPLE_ID:-$SIGNING_NOTARY_APPLE_ID}
SIGNING_NOTARY_TEAM_ID=${LUDORK_MACOS_NOTARY_TEAM_ID:-$SIGNING_NOTARY_TEAM_ID}
SIGNING_NOTARY_KEY=${LUDORK_MACOS_NOTARY_KEY:-$SIGNING_NOTARY_KEY}
SIGNING_NOTARY_KEY_ID=${LUDORK_MACOS_NOTARY_KEY_ID:-$SIGNING_NOTARY_KEY_ID}
SIGNING_NOTARY_KEY_ISSUER=${LUDORK_MACOS_NOTARY_KEY_ISSUER:-$SIGNING_NOTARY_KEY_ISSUER}
SIGNING_CERTIFICATE_PASSWORD=
SIGNING_NOTARY_PASSWORD=
if [ -n "$SIGNING_CERTIFICATE" ]; then
    SIGNING_CERTIFICATE_PASSWORD=$(read_signing_secret)
fi
if [ "$SIGNING_NOTARIZE" -eq 1 ] && [ -n "$SIGNING_NOTARY_APPLE_ID" ]; then
    SIGNING_NOTARY_PASSWORD=$(read_signing_secret)
fi

PROJECT_FILE="$PROJECT_ROOT/Ludork.csproj"
WORK_DIR="$PROJECT_ROOT/obj/editor-package"
STAGE_DIR="$WORK_DIR/dist"
DMG_ROOT="$WORK_DIR/dmg-root"
DMG_MOUNT_DIR="/tmp/ludork-editor-dmg-mount.$$"
DMG_READ_WRITE="/tmp/ludork-editor-layout-rw.$$.dmg"
DMG_PAYLOAD_ROOT="$DMG_ROOT/.official-plugin-payload"
DMG_BACKGROUND_DIR="$DMG_ROOT/.background"
DMG_INSTALLER="$DMG_ROOT/Install Official Plugins.command"
DMG_ASSET_DIR="$PROJECT_ROOT/tools/editor_dmg"
APP_DIR="$DMG_ROOT/Ludork.app"
MACOS_DIR="$APP_DIR/Contents/MacOS"
RESOURCES_DIR="$APP_DIR/Contents/Resources"
FINAL_DIR="$PROJECT_ROOT/dist"
BACKUP_DIR="$WORK_DIR/previous-dist"
SCRIPT_TOOLS_DIRECTORY="$PROJECT_ROOT/.tools/ScriptTools"
SCRIPT_TOOLS="$SCRIPT_TOOLS_DIRECTORY/ScriptTools"
EDITOR_CACHE_DIRECTORY=$("$SCRIPT_TOOLS" packaging-constants list editor-cache-directory --separator space)
TEMPLATE_NAMES=$("$SCRIPT_TOOLS" packaging-constants list template-names --separator space)
CPP_TEMPLATE_NAMES=$("$SCRIPT_TOOLS" packaging-constants list cpp-template-names --separator space)
STANDALONE_TEMPLATE_NAMES=$("$SCRIPT_TOOLS" packaging-constants list standalone-template-names --separator space)
PLAIN_TEMPLATE_NAMES=$("$SCRIPT_TOOLS" packaging-constants list plain-template-names --separator space)
FFMPEG_TEMPLATE_NAMES=$("$SCRIPT_TOOLS" packaging-constants list ffmpeg-template-names --separator space)
SCRIPT_TOOLS_VERSION_REPORT="$SCRIPT_TOOLS_DIRECTORY/runtime-versions.txt"
FFMPEG_SOURCE_ARCHIVE="$PROJECT_ROOT/Game/ThirdPartySource/ffmpeg-$FFMPEG_VERSION.tar.gz"
LUAC="$PROJECT_ROOT/.tools/Lua/luac"
DIST_BACKED_UP=0
DMG_MOUNTED=0
DMG_IMAGE_PATH=
DMG_MOUNT_DIR_OWNED=0
DMG_TEMP_OWNED=0
WORK_DIR_OWNED=0

cleanup() {
    status=$?
    if [ "$status" -eq 0 ]; then
        return
    fi

    set +e
    remove_work_dir=$WORK_DIR_OWNED
    if [ "$DMG_MOUNTED" -eq 1 ]; then
        if ! detach_dmg; then
            remove_work_dir=0
            echo "Failed to detach the editor DMG; preserving the work directory: $WORK_DIR" >&2
        fi
    fi
    if [ "$DIST_BACKED_UP" -eq 1 ]; then
        restore_ready=1
        if [ -e "$FINAL_DIR" ] || [ -L "$FINAL_DIR" ]; then
            if ! rm -rf "$FINAL_DIR"; then
                restore_ready=0
            fi
        fi
        if [ -e "$FINAL_DIR" ] || [ -L "$FINAL_DIR" ]; then
            restore_ready=0
        fi
        if [ "$restore_ready" -eq 1 ] && mv "$BACKUP_DIR" "$FINAL_DIR"; then
            DIST_BACKED_UP=0
        else
            remove_work_dir=0
            echo "Failed to restore the previous editor package; preserving its backup: $BACKUP_DIR" >&2
        fi
    fi
    if [ "$remove_work_dir" -eq 1 ] && [ -d "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR"
    fi
    if [ "$DMG_MOUNTED" -eq 0 ] && [ "$DMG_MOUNT_DIR_OWNED" -eq 1 ] \
        && [ -d "$DMG_MOUNT_DIR" ]; then
        rmdir "$DMG_MOUNT_DIR" 2>/dev/null || true
    fi
    if [ "$DMG_MOUNTED" -eq 0 ] && [ "$DMG_TEMP_OWNED" -eq 1 ] \
        && [ -f "$DMG_READ_WRITE" ]; then
        unlink "$DMG_READ_WRITE" 2>/dev/null || true
    fi
    echo "Editor packaging failed." >&2
}

interrupt() {
    exit 130
}

trap cleanup EXIT
trap interrupt HUP INT TERM

attach_dmg() {
    DMG_IMAGE_PATH=$1
    DMG_MOUNTED=1
    hdiutil attach "$2" -nobrowse \
        -mountpoint "$DMG_MOUNT_DIR" "$DMG_IMAGE_PATH" >/dev/null
}

attached_dmg_device() {
    dmg_info="$WORK_DIR/dmg-info.plist"
    hdiutil info -plist > "$dmg_info" || return 1
    dmg_image_count=$(plutil -extract images raw -expect array -o - "$dmg_info") || return 1
    dmg_image_index=0
    while [ "$dmg_image_index" -lt "$dmg_image_count" ]; do
        dmg_image_path=$(plutil -extract "images.$dmg_image_index.image-path" \
            raw -expect string -o - "$dmg_info") || return 1
        if [ "$dmg_image_path" -ef "$DMG_IMAGE_PATH" ]; then
            plutil -extract "images.$dmg_image_index.system-entities.0.dev-entry" \
                raw -expect string -o - "$dmg_info"
            return $?
        fi
        dmg_image_index=$((dmg_image_index + 1))
    done
}

detach_dmg() {
    dmg_detach_attempt=0
    while :; do
        dmg_device=$(attached_dmg_device) || return 1
        if [ -z "$dmg_device" ]; then
            DMG_MOUNTED=0
            return 0
        fi
        if [ "$dmg_detach_attempt" -ge 4 ]; then
            echo "Editor disk image is still attached: $DMG_IMAGE_PATH ($dmg_device)" >&2
            return 1
        fi
        if [ "$dmg_detach_attempt" -eq 0 ]; then
            hdiutil detach "$dmg_device" >/dev/null || true
        else
            echo "Retrying editor DMG detach with force: $dmg_device..." >&2
            hdiutil detach -force "$dmg_device" >/dev/null || true
        fi
        dmg_detach_attempt=$((dmg_detach_attempt + 1))
        sleep 2
    done
}

require_file() {
    if [ -f "$1" ]; then
        return
    fi
    echo "Required file was not found: $1" >&2
    exit 1
}

require_directory() {
    if [ -d "$1" ]; then
        return
    fi
    echo "Required directory was not found: $1" >&2
    exit 1
}

require_command() {
    if command -v "$1" >/dev/null 2>&1; then
        return
    fi
    echo "Required command was not found: $1" >&2
    exit 1
}

# Modes: app signs the assembled editor bundle, dmg signs the disk image, and
# dmg-notarize signs the disk image and then notarises and staples it. Signing
# passwords never reach the command line; they are piped on standard input in
# the order the signing tool reads them.
run_macos_sign() {
    signing_mode=$1
    signing_target=$2
    set --
    if [ -n "$SIGNING_IDENTITY" ]; then
        set -- "$@" --signing-identity "$SIGNING_IDENTITY"
    fi
    if [ -n "$SIGNING_CERTIFICATE" ]; then
        set -- "$@" --certificate "$SIGNING_CERTIFICATE"
    fi
    if [ -n "$SIGNING_NOTARY_APPLE_ID" ]; then
        set -- "$@" --notary-apple-id "$SIGNING_NOTARY_APPLE_ID"
    fi
    if [ -n "$SIGNING_NOTARY_TEAM_ID" ]; then
        set -- "$@" --notary-team-id "$SIGNING_NOTARY_TEAM_ID"
    fi
    if [ -n "$SIGNING_NOTARY_KEY" ]; then
        set -- "$@" --notary-key "$SIGNING_NOTARY_KEY"
    fi
    if [ -n "$SIGNING_NOTARY_KEY_ID" ]; then
        set -- "$@" --notary-key-id "$SIGNING_NOTARY_KEY_ID"
    fi
    if [ -n "$SIGNING_NOTARY_KEY_ISSUER" ]; then
        set -- "$@" --notary-key-issuer "$SIGNING_NOTARY_KEY_ISSUER"
    fi
    if [ "$signing_mode" = "app" ]; then
        set -- "$@" --entitlements "$SIGNING_ENTITLEMENTS"
        set -- "$@" --runtime-bundle "$RESOURCES_DIR/tools/ScriptTools"
        if [ "$WITHOUT_TEMPLATES" -eq 0 ]; then
            for template_name in $STANDALONE_TEMPLATE_NAMES; do
                set -- "$@" --ui-preview-template "$RESOURCES_DIR/Templates/$template_name"
            done
        fi
    fi
    if [ "$signing_mode" = "dmg-notarize" ]; then
        set -- "$@" --notarize
    fi
    {
        if [ -n "$SIGNING_CERTIFICATE" ]; then
            printf '%s\n' "$SIGNING_CERTIFICATE_PASSWORD"
        fi
        if [ "$signing_mode" = "dmg-notarize" ] && [ -n "$SIGNING_NOTARY_APPLE_ID" ]; then
            printf '%s\n' "$SIGNING_NOTARY_PASSWORD"
        fi
    } | "$SCRIPT_TOOLS" macos-sign "$@" "$signing_target"
}

copy_directory() {
    source_dir=$1
    target_dir=$2
    mkdir -p "$target_dir"
    rsync -a --delete --exclude '.DS_Store' "$source_dir/" "$target_dir/"
}

copy_public_docs() {
    docs_target_dir=$1
    rm -rf "$docs_target_dir"
    mkdir -p "$docs_target_dir"
    copy_directory "$PROJECT_ROOT/docs/_images" "$docs_target_dir/_images"
    copy_directory "$PROJECT_ROOT/docs/en_GB" "$docs_target_dir/en_GB"
    copy_directory "$PROJECT_ROOT/docs/zh_CN" "$docs_target_dir/zh_CN"
}

copy_about_files() {
    target_dir=$1
    found=0
    for source_path in "$PROJECT_ROOT"/docs/About_*.md; do
        if [ ! -f "$source_path" ]; then
            continue
        fi
        cp "$source_path" "$target_dir/$(basename -- "$source_path")"
        found=1
    done
    if [ "$found" -ne 1 ]; then
        echo "No About_*.md resources were found." >&2
        exit 1
    fi
}

create_icns() {
    icon_source=$1
    icon_name=$2
    icon_work_dir="$WORK_DIR/editor-icons/$icon_name"
    iconset_dir="$icon_work_dir/$icon_name.iconset"
    source_png="$icon_work_dir/source.png"
    mkdir -p "$iconset_dir" "$RESOURCES_DIR"
    sips -s format png "$icon_source" --out "$source_png" >/dev/null
    for size in 16 32 128 256 512; do
        sips -z "$size" "$size" "$source_png" \
            --out "$iconset_dir/icon_${size}x${size}.png" >/dev/null
        retina_size=$((size * 2))
        sips -z "$retina_size" "$retina_size" "$source_png" \
            --out "$iconset_dir/icon_${size}x${size}@2x.png" >/dev/null
    done
    iconutil -c icns "$iconset_dir" -o "$RESOURCES_DIR/$icon_name.icns"
    require_file "$RESOURCES_DIR/$icon_name.icns"
}

create_bundle_icons() {
    create_icns "$PROJECT_ROOT/Editor/Assets/icon.ico" AppIcon
    create_icns "$PROJECT_ROOT/Editor/Assets/project-icon.png" ProjectIcon
}

create_dmg_assets() {
    dmg_icon_work_dir="$WORK_DIR/editor-dmg-installer-icon"
    dmg_icon_png="$dmg_icon_work_dir/installer-icon.png"
    dmg_icon_resource="$dmg_icon_work_dir/installer-icon.rsrc"

    mkdir -p "$DMG_BACKGROUND_DIR" "$dmg_icon_work_dir"
    sips -s format png \
        "$DMG_ASSET_DIR/background.svg" \
        --out "$DMG_BACKGROUND_DIR/background.png" \
        >/dev/null
    cp "$DMG_ASSET_DIR/install_official_plugins.command" "$DMG_INSTALLER"
    cp "$RESOURCES_DIR/AppIcon.icns" "$DMG_ROOT/.VolumeIcon.icns"
    touch "$DMG_ROOT/.metadata_never_index"

    sips -s format png \
        "$DMG_ASSET_DIR/installer_icon.svg" \
        --out "$dmg_icon_png" \
        >/dev/null
    sips -z 512 512 "$dmg_icon_png" >/dev/null
    sips -i "$dmg_icon_png" >/dev/null
    xattr -c "$DMG_BACKGROUND_DIR/background.png"
    xattr -c "$DMG_INSTALLER"
    xattr -c "$DMG_ROOT/.VolumeIcon.icns"
    xattr -c "$DMG_ROOT/.metadata_never_index"
    DeRez -only icns "$dmg_icon_png" > "$dmg_icon_resource"
    Rez -append "$dmg_icon_resource" -o "$DMG_INSTALLER"

    chmod +x "$DMG_INSTALLER"
    SetFile -a CE "$DMG_INSTALLER"
    SetFile -c icnC "$DMG_ROOT/.VolumeIcon.icns"
    SetFile -a V "$DMG_BACKGROUND_DIR"
    SetFile -a V "$DMG_PAYLOAD_ROOT"
    SetFile -a V "$DMG_ROOT/.VolumeIcon.icns"
    SetFile -a V "$DMG_ROOT/.metadata_never_index"
}

purge_python_cache() {
    target_dir=$1
    find "$target_dir" -type d -name '__pycache__' -prune -exec rm -rf {} +
    find "$target_dir" -type f \( -name '*.pyc' -o -name '*.pyo' \) -delete
}

purge_debug_symbols() {
    target_dir=$1
    find "$target_dir" -type f -name '*.pdb' -delete
}

purge_windows_tools() {
    target_dir=$1
    find "$target_dir" -type f -name '*.bat' -delete
}

purge_template_runtime_state() {
    templates_dir=$1
    for template_name in $TEMPLATE_NAMES; do
        template_dir="$templates_dir/$template_name"
        rm -rf "$template_dir/Log" "$template_dir/Save"
        rm -f \
            "$template_dir/Main.ini" \
            "$template_dir/Ludork.ini" \
            "$template_dir/Ludork-startup-error.log"
    done
}

require_package_file() {
    package_path=$1
    if [ -f "$package_path" ]; then
        return
    fi
    echo "Required package file was not found: $package_path" >&2
    exit 1
}

require_package_directory() {
    package_path=$1
    if [ -d "$package_path" ]; then
        return
    fi
    echo "Required package directory was not found: $package_path" >&2
    exit 1
}

require_package_executable() {
    package_path=$1
    if [ -x "$package_path" ]; then
        return
    fi
    echo "Required package executable was not found: $package_path" >&2
    exit 1
}

validate_absent_pattern() {
    package_dir=$1
    pattern=$2
    forbidden_path=$(find "$package_dir" -name "$pattern" -print -quit)
    if [ -n "$forbidden_path" ]; then
        echo "Forbidden package entry was found: $forbidden_path" >&2
        exit 1
    fi
}

validate_dmg_root() {
    dmg_validation_root=$1
    dmg_layout_required=$2
    dmg_applications_link="$dmg_validation_root/Applications"
    dmg_validation_installer="$dmg_validation_root/Install Official Plugins.command"
    dmg_validation_payload="$dmg_validation_root/.official-plugin-payload"
    dmg_validation_background="$dmg_validation_root/.background"

    require_package_directory "$dmg_validation_root/Ludork.app"
    if [ ! -L "$dmg_applications_link" ]; then
        echo "The Applications link was not found in the DMG root: $dmg_applications_link" >&2
        exit 1
    fi
    if [ "$(readlink "$dmg_applications_link")" != "/Applications" ]; then
        echo "The Applications link target is invalid: $dmg_applications_link" >&2
        exit 1
    fi
    require_package_executable "$dmg_validation_installer"
    if [ -L "$dmg_validation_installer" ] || ! cmp -s \
        "$DMG_ASSET_DIR/install_official_plugins.command" \
        "$dmg_validation_installer"; then
        echo "The DMG official plug-in installer is invalid: $dmg_validation_installer" >&2
        exit 1
    fi
    if ! DeRez -only icns "$dmg_validation_installer" >/dev/null 2>&1; then
        echo "The DMG official plug-in installer icon is missing." >&2
        exit 1
    fi
    dmg_installer_attributes=$(GetFileInfo -a "$dmg_validation_installer")
    case "$dmg_installer_attributes" in
        *C*) ;;
        *)
            echo "The DMG official plug-in installer custom icon flag is missing." >&2
            exit 1
            ;;
    esac
    case "$dmg_installer_attributes" in
        *E*) ;;
        *)
            echo "The DMG official plug-in installer extension is not hidden." >&2
            exit 1
            ;;
    esac

    require_package_directory "$dmg_validation_payload"
    require_package_directory "$dmg_validation_background"
    require_package_file "$dmg_validation_background/background.png"
    require_package_file "$dmg_validation_root/.VolumeIcon.icns"
    require_package_file "$dmg_validation_root/.metadata_never_index"
    for dmg_regular_path in \
        "$dmg_validation_payload" \
        "$dmg_validation_background" \
        "$dmg_validation_background/background.png" \
        "$dmg_validation_root/.VolumeIcon.icns" \
        "$dmg_validation_root/.metadata_never_index"; do
        if [ -L "$dmg_regular_path" ]; then
            echo "A symbolic link was found in the DMG support files: $dmg_regular_path" >&2
            exit 1
        fi
    done

    "$SCRIPT_TOOLS" editor-official-plugins validate "$dmg_validation_payload"
    validate_package "$dmg_validation_root/Ludork.app"

    dmg_unexpected_background_path=$(find "$dmg_validation_background" \
        -mindepth 1 \
        -maxdepth 1 \
        ! -name background.png \
        -print \
        -quit)
    if [ -n "$dmg_unexpected_background_path" ]; then
        echo "Unexpected DMG background entry was found: $dmg_unexpected_background_path" >&2
        exit 1
    fi
    dmg_background_width=$(sips -g pixelWidth "$dmg_validation_background/background.png" |
        awk '/pixelWidth:/ { print $2 }')
    dmg_background_height=$(sips -g pixelHeight "$dmg_validation_background/background.png" |
        awk '/pixelHeight:/ { print $2 }')
    if [ "$dmg_background_width" != "760" ] || [ "$dmg_background_height" != "500" ]; then
        echo "The DMG background dimensions are invalid." >&2
        exit 1
    fi
    if ! file "$dmg_validation_background/background.png" | grep -q "PNG image data"; then
        echo "The DMG background image is invalid." >&2
        exit 1
    fi
    if ! file "$dmg_validation_root/.VolumeIcon.icns" | grep -q "Mac OS X icon"; then
        echo "The DMG volume icon is invalid." >&2
        exit 1
    fi
    if [ -s "$dmg_validation_root/.metadata_never_index" ]; then
        echo "The DMG Spotlight marker is invalid." >&2
        exit 1
    fi
    for dmg_clean_metadata_path in \
        "$dmg_validation_installer" \
        "$dmg_validation_background/background.png" \
        "$dmg_validation_root/.VolumeIcon.icns" \
        "$dmg_validation_root/.metadata_never_index"; do
        if xattr -p com.apple.quarantine "$dmg_clean_metadata_path" >/dev/null 2>&1; then
            echo "Unexpected quarantine metadata was found in the DMG: $dmg_clean_metadata_path" >&2
            exit 1
        fi
    done

    if [ "$dmg_layout_required" -eq 1 ]; then
        require_package_file "$dmg_validation_root/.DS_Store"
        if [ -L "$dmg_validation_root/.DS_Store" ]; then
            echo "The DMG Finder layout is a symbolic link." >&2
            exit 1
        fi
        if ! file "$dmg_validation_root/.DS_Store" | grep -q "Apple Desktop Services Store"; then
            echo "The DMG Finder layout is invalid." >&2
            exit 1
        fi
        if ! LC_ALL=C strings "$dmg_validation_root/.DS_Store" | grep -q 'background.png'; then
            echo "The DMG Finder layout does not reference its background." >&2
            exit 1
        fi
        if LC_ALL=C strings "$dmg_validation_root/.DS_Store" | grep -Fq "$PROJECT_ROOT"; then
            echo "The DMG Finder layout contains the packaging workspace path." >&2
            exit 1
        fi
        dmg_volume_attributes=$(GetFileInfo -a "$dmg_validation_root")
        case "$dmg_volume_attributes" in
            *C*) ;;
            *)
                echo "The DMG custom volume icon flag is missing." >&2
                exit 1
                ;;
        esac
    elif [ -e "$dmg_validation_root/.DS_Store" ] || [ -L "$dmg_validation_root/.DS_Store" ]; then
        echo "The staged DMG contains a Finder layout before image creation." >&2
        exit 1
    fi

    unexpected_root_path=$(find "$dmg_validation_root" \
        -mindepth 1 \
        -maxdepth 1 \
        ! -name Ludork.app \
        ! -name Applications \
        ! -name 'Install Official Plugins.command' \
        ! -name .official-plugin-payload \
        ! -name .background \
        ! -name .DS_Store \
        ! -name .VolumeIcon.icns \
        ! -name .metadata_never_index \
        -print \
        -quit)
    if [ -n "$unexpected_root_path" ]; then
        echo "Unexpected DMG root entry was found: $unexpected_root_path" >&2
        exit 1
    fi
}

validate_dmg() {
    dmg_path=$1

    require_package_file "$dmg_path"
    hdiutil verify "$dmg_path" >/dev/null
    if [ -e "$DMG_MOUNT_DIR" ] || [ -L "$DMG_MOUNT_DIR" ]; then
        echo "The editor DMG mount point already exists: $DMG_MOUNT_DIR" >&2
        exit 1
    fi
    mkdir "$DMG_MOUNT_DIR"
    DMG_MOUNT_DIR_OWNED=1
    attach_dmg "$dmg_path" -readonly
    validate_dmg_root "$DMG_MOUNT_DIR" 1
    detach_dmg
    rmdir "$DMG_MOUNT_DIR"
    DMG_MOUNT_DIR_OWNED=0
}

validate_standalone_runtime_layout() {
    template_dir=$1
    require_package_executable "$template_dir/Main"
    require_package_directory "$template_dir/Binaries"
    if [ -e "$template_dir/Binaries/Main" ] \
        || [ -L "$template_dir/Binaries/Main" ]; then
        echo "Standalone Main must remain at the template root: $template_dir/Binaries/Main" >&2
        exit 1
    fi
    unexpected_runtime=$(find "$template_dir" \
        -maxdepth 1 \
        \( -type f -o -type l \) \
        \( -name '*.dll' -o -name '*.so' -o -name '*.so.*' -o -name '*.dylib' \) \
        -print \
        -quit)
    if [ -n "$unexpected_runtime" ]; then
        echo "Standalone runtime library exists outside Binaries: $unexpected_runtime" >&2
        exit 1
    fi
    runtime_library=$(find "$template_dir/Binaries" \
        -maxdepth 1 \
        \( -type f -o -type l \) \
        \( -name '*.so' -o -name '*.so.*' -o -name '*.dylib' \) \
        -print \
        -quit)
    if [ -z "$runtime_library" ]; then
        echo "Standalone template contains no runtime libraries in Binaries: $template_dir" >&2
        exit 1
    fi
    if ! otool -l "$template_dir/Main" \
        | grep -Fq 'path @loader_path/Binaries '; then
        echo "Standalone Main is missing the Binaries runtime search path: $template_dir/Main" >&2
        exit 1
    fi
    if otool -l "$template_dir/Main" \
        | grep -Fq 'path @loader_path/../Frameworks '; then
        echo "Raw Standalone Main contains the app-only Frameworks runtime search path: $template_dir/Main" >&2
        exit 1
    fi
    if otool -l "$template_dir/Main" \
        | grep -Fq 'path @loader_path '; then
        echo "Raw Standalone Main contains the development runtime search path: $template_dir/Main" >&2
        exit 1
    fi
    codesign --verify --strict "$template_dir/Main"
}

validate_script_tools_bundle() {
    script_tools_bundle=$1
    require_package_directory "$script_tools_bundle"
    require_package_executable "$script_tools_bundle/ScriptTools"
    require_package_file "$script_tools_bundle/runtime-versions.txt"
    require_package_file "$script_tools_bundle/runtime-files.json"
    "$script_tools_bundle/ScriptTools" runtime-bundle validate "$script_tools_bundle"
    find "$script_tools_bundle" -type f -exec sh -c '
        for binary do
            case "$(file -b "$binary")" in
                *Mach-O*) codesign --verify --strict "$binary" || exit 1 ;;
            esac
        done
    ' sh {} +
}

validate_package() {
    package_app=$1
    package_macos="$package_app/Contents/MacOS"
    package_resources="$package_app/Contents/Resources"
    info_plist="$package_app/Contents/Info.plist"

    require_package_executable "$package_macos/Ludork"
    if [ -n "$SIGNING_IDENTITY$SIGNING_CERTIFICATE" ]; then
        codesign --verify --deep --strict "$package_app"
    fi
    require_package_file "$info_plist"
    require_package_file "$package_resources/BuildInfo.json"
    require_package_file "$package_resources/AppIcon.icns"
    require_package_file "$package_resources/ProjectIcon.icns"
    require_package_file "$package_resources/Locale/en_GB"
    require_package_file "$package_resources/Locale/zh_CN"
    if [ "$WITHOUT_TEMPLATES" -eq 1 ]; then
        if [ -e "$package_resources/Templates" ] || [ -L "$package_resources/Templates" ]; then
            echo "Unexpected Templates entry in an editor-only package: $package_resources/Templates" >&2
            exit 1
        fi
    else
        require_package_file "$package_resources/Templates/Cpp/Main.proj"
        require_package_file "$package_resources/Templates/Cpp-ffmpeg/Main.proj"
        validate_standalone_runtime_layout \
            "$package_resources/Templates/Standalone"
        validate_standalone_runtime_layout \
            "$package_resources/Templates/Standalone-ffmpeg"
        for template_name in $TEMPLATE_NAMES; do
            "$SCRIPT_TOOLS" validate-ldpak-source \
                "$package_resources/Templates/$template_name"
        done
    fi
    require_package_executable "$package_resources/tools/build_cpp.sh"
    require_package_executable "$package_resources/tools/build_standalone.sh"
    require_package_executable "$package_resources/tools/pack_project.sh"
    require_package_executable "$package_resources/tools/pack_ios.sh"
    require_package_executable "$package_resources/tools/pack_harmony.sh"
    require_package_executable "$package_resources/tools/pack_android.sh"
    require_package_executable "$package_resources/tools/unregister_editor.sh"
    require_package_file "$package_resources/tools/common.sh"
    validate_script_tools_bundle "$package_resources/tools/ScriptTools"
    require_package_executable "$package_resources/tools/luac"
    require_package_executable "$package_resources/tools/build_ui_preview_host.sh"
    if [ "$WITHOUT_TEMPLATES" -eq 0 ]; then
        for template_name in $STANDALONE_TEMPLATE_NAMES; do
            "$SCRIPT_TOOLS" ui-preview validate "$package_resources/Templates/$template_name"
            unexpected_temp=$(find "$package_resources/Templates/$template_name/$EDITOR_CACHE_DIRECTORY" \
                -mindepth 1 -maxdepth 1 ! -name UiPreview.json ! -name UiPreview.registry.json -print -quit)
            if [ -n "$unexpected_temp" ]; then
                echo "Unexpected template $EDITOR_CACHE_DIRECTORY entry: $unexpected_temp" >&2
                exit 1
            fi
        done
        for template_name in $CPP_TEMPLATE_NAMES; do
            require_package_file "$package_resources/Templates/$template_name/Engine/UiPreviewHost/CMakeLists.txt"
            for wrapper_path in \
                gradlew \
                gradle/wrapper/gradle-wrapper.jar \
                gradle/wrapper/gradle-wrapper.properties \
                gradle/wrapper/LICENSE.txt \
                gradle/wrapper/NOTICE.md; do
                require_package_file "$package_resources/Templates/$template_name/Engine/PlatformHosts/Android/$wrapper_path"
            done
            if [ -e "$package_resources/Templates/$template_name/Binaries" ] \
                || [ -e "$package_resources/Templates/$template_name/$EDITOR_CACHE_DIRECTORY" ]; then
                echo "Source template contains a prebuilt UI preview snapshot." >&2
                exit 1
            fi
        done
    fi
    require_package_file "$package_resources/LICENSE.md"
    require_package_file "$package_resources/README.md"
    require_package_file "$package_resources/README_zh_CN.md"
    require_package_file "$package_resources/docs/THIRD_PARTY_NOTICES.md"
    require_package_file "$package_resources/docs/THIRD_PARTY_NOTICES_zh_CN.md"
    require_package_directory "$package_resources/docs/_images"
    require_package_directory "$package_resources/docs/en_GB"
    require_package_directory "$package_resources/docs/zh_CN"
    require_package_directory "$package_resources/Licenses"

    for required_licence_path in \
        DotNet/LICENSE.txt \
        DotNet/THIRD-PARTY-NOTICES.txt \
        DotNetPackages/Microsoft.Extensions-8.0.0-LICENSE.txt \
        DotNetPackages/Microsoft.Extensions-8.0.0-THIRD-PARTY-NOTICES.txt \
        DotNetPackages/Microsoft.Win32.Registry-and-System.Security-4.7.0-LICENSE.txt \
        DotNetPackages/Microsoft.Win32.Registry-and-System.Security-4.7.0-THIRD-PARTY-NOTICES.txt \
        DotNetPackages/System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-LICENSE.txt \
        DotNetPackages/System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-THIRD-PARTY-NOTICES.txt \
        DotNetPackages/System.IO.Pipelines-8.0.0-LICENSE.txt \
        DotNetPackages/System.IO.Pipelines-8.0.0-THIRD-PARTY-NOTICES.txt \
        DotNetPackages/System.Memory-4.5.3-LICENSE.txt \
        DotNetPackages/System.Memory-4.5.3-THIRD-PARTY-NOTICES.txt \
        DotNetPackages/System.ValueTuple-4.5.0-LICENSE.txt \
        DotNetPackages/System.ValueTuple-4.5.0-THIRD-PARTY-NOTICES.txt \
        README.md \
        README_zh_CN.md \
        Avalonia/ANGLE-LICENSE.txt \
        Avalonia/LICENSE.txt \
        Avalonia/Inter-OFL-1.1.txt \
        EditorPackages/AvaloniaEdit-LICENSE.txt \
        EditorPackages/CommunityToolkit.Mvvm-LICENSE.md \
        EditorPackages/Svg.Skia-LICENSE.txt \
        EditorPackages/Svg.Custom-LICENSE.txt \
        EditorPackages/ExCSS-LICENSE.txt \
        EditorPackages/CommunityToolkit.Mvvm-THIRD-PARTY-NOTICES.txt \
        EditorPackages/HarfBuzzSharp-LICENSE.txt \
        EditorPackages/MoonSharp-LICENSE.txt \
        EditorPackages/NAudio-LICENSE.txt \
        EditorPackages/NAudio.Vorbis-LICENSE.txt \
        EditorPackages/NVorbis-LICENSE.txt \
        EditorPackages/NodifyM.Avalonia-LICENSE.txt \
        EditorPackages/Roslyn-LICENSE.txt \
        EditorPackages/Roslyn-THIRD-PARTY-NOTICES.rtf \
        EditorPackages/SkiaSharp-LICENSE.txt \
        EditorPackages/System.Reactive-LICENSE.txt \
        EditorPackages/MicroCom.Runtime-LICENSE.txt \
        EditorPackages/Microsoft.IO.RecyclableMemoryStream-LICENSE.txt \
        EditorPackages/SkiaSharp-and-HarfBuzzSharp-NativeAssets-THIRD-PARTY-NOTICES.txt \
        EditorPackages/Tmds.DBus.Protocol-LICENSE.txt \
        LuaSF/LICENSE.txt \
        Lua/LICENSE.txt \
        SFML/LICENSE.txt \
        LuaGlue/LICENSE \
        lua-cjson/LICENSE.txt \
        zlib/LICENSE.txt \
        FFmpeg/COPYING.GPLv2.txt \
        FFmpeg/COPYING.GPLv3.txt \
        FFmpeg/COPYING.LGPLv2.1.txt \
        FFmpeg/COPYING.LGPLv3.txt \
        FFmpeg/README.md \
        FFmpeg/UPSTREAM-LICENSE.md \
        GNUMake/COPYING.txt \
        MicrosoftVisualCppRuntime/README.md \
        MicrosoftVisualCppRuntime/Visual-C-Runtime-2015-2022-License.docx \
        NativeDependencies/FreeType-FTL.txt \
        NativeDependencies/FreeType-LICENSE.txt \
        NativeDependencies/Glad-CC0-1.0.txt \
        NativeDependencies/HarfBuzz-COPYING.txt \
        NativeDependencies/SheenBidi-LICENSE.txt \
        NativeDependencies/Ogg-COPYING.txt \
        NativeDependencies/Vorbis-COPYING.txt \
        NativeDependencies/Wine-DInput-LGPLv2.1.txt \
        NativeDependencies/FLAC-COPYING.Xiph.txt \
        NativeDependencies/MbedTLS-LICENSE.txt \
        NativeDependencies/libssh2-COPYING.txt \
        NativeDependencies/SFML-THIRD-PARTY.md \
        HarmonyOSSans/LICENSE.txt \
        SampleMusic/NOTICE.md \
        ScriptTools/Nuitka-4.1.3-AGPL-3.0.txt \
        ScriptTools/Nuitka-4.1.3-NOTICE.txt \
        ScriptTools/Nuitka-4.1.3-RUNTIME-EXCEPTION.txt \
        ScriptTools/Python-3.12-LICENSES-AND-ACKNOWLEDGEMENTS.rst.txt \
        ScriptTools/Pillow-12.2.0-LICENSE.txt; do
        require_package_file "$package_resources/Licenses/$required_licence_path"
    done

    unexpected_docs_path=$(find "$package_resources/docs" \
        -mindepth 1 \
        -maxdepth 1 \
        ! -name _images \
        ! -name en_GB \
        ! -name zh_CN \
        ! -name 'About_*.md' \
        ! -name 'THIRD_PARTY_NOTICES*.md' \
        -print \
        -quit)
    if [ -n "$unexpected_docs_path" ]; then
        echo "Non-public documentation was found in the editor package: $unexpected_docs_path" >&2
        exit 1
    fi

    for source_path in "$PROJECT_ROOT"/docs/About_*.md; do
        if [ -f "$source_path" ]; then
            require_package_file "$package_resources/docs/$(basename -- "$source_path")"
        fi
    done
    require_package_file "$package_resources/docs/About_en_GB.md"
    require_package_file "$package_resources/docs/About_zh_CN.md"

    plutil -lint "$info_plist" >/dev/null
    if [ "$(plutil -extract CFBundleExecutable raw -o - "$info_plist")" != "Ludork" ]; then
        echo "The editor bundle executable metadata is invalid." >&2
        exit 1
    fi
    if [ "$(plutil -extract CFBundleIdentifier raw -o - "$info_plist")" != "com.ludork.editor" ]; then
        echo "The editor bundle identifier is invalid." >&2
        exit 1
    fi
    if [ "$(plutil -extract LSMinimumSystemVersion raw -o - "$info_plist")" != "13.3" ]; then
        echo "The editor minimum macOS version is invalid." >&2
        exit 1
    fi
    if [ "$(/usr/bin/lipo -archs "$package_macos/Ludork")" != "arm64" ]; then
        echo "The editor executable is not Apple Silicon-only." >&2
        exit 1
    fi
    if ! file "$package_resources/AppIcon.icns" | grep -q "Mac OS X icon"; then
        echo "The editor bundle icon is invalid." >&2
        exit 1
    fi
    if ! file "$package_resources/ProjectIcon.icns" | grep -q "Mac OS X icon"; then
        echo "The Ludork project icon is invalid." >&2
        exit 1
    fi
    "$SCRIPT_TOOLS" editor-macos-metadata validate "$info_plist" \
        --build-info "$package_resources/BuildInfo.json"

    for forbidden_path in \
        "$package_macos/Game" \
        "$package_macos/.ludork-development" \
        "$package_macos/requirements.txt" \
        "$package_macos/versions.conf" \
        "$package_macos/.venv" \
        "$package_macos/.tools" \
        "$package_macos/About_en_GB.md" \
        "$package_macos/About_zh_CN.md" \
        "$package_macos/LICENSE.md" \
        "$package_macos/README.md" \
        "$package_macos/README_zh_CN.md" \
        "$package_macos/THIRD_PARTY_NOTICES.md" \
        "$package_macos/THIRD_PARTY_NOTICES_zh_CN.md" \
        "$package_macos/Locale" \
        "$package_macos/Templates" \
        "$package_macos/docs" \
        "$package_macos/Page" \
        "$package_macos/Licenses" \
        "$package_macos/tools" \
        "$package_macos/Ludork.ini" \
        "$package_resources/Locale/locale.json" \
        "$package_resources"/About_*.md \
        "$package_resources"/THIRD_PARTY_NOTICES*.md \
        "$package_resources/Page" \
        "$package_resources/tools/ScriptTools-runtime-versions.txt" \
        "$package_resources/tools/pack_editor.sh" \
        "$package_resources/tools/pack_editor.bat" \
        "$package_resources/tools/pack_editor_msi.bat" \
        "$package_resources/tools/create_templates.sh" \
        "$package_resources/tools/run_editor.sh" \
        "$package_resources/tools/installer"; do
        if [ -e "$forbidden_path" ] || [ -L "$forbidden_path" ]; then
            echo "Forbidden package entry was found: $forbidden_path" >&2
            exit 1
        fi
    done

    bundled_plugin_path=$(find "$package_app" \
        \( -name Plugins -o -name plugins.json \) \
        -print \
        -quit)
    if [ -n "$bundled_plugin_path" ]; then
        echo "Plugin payload was found inside the editor app: $bundled_plugin_path" >&2
        exit 1
    fi

    validate_absent_pattern "$package_app" '__pycache__'
    validate_absent_pattern "$package_app" '*.py'
    validate_absent_pattern "$package_app" '*.pyc'
    validate_absent_pattern "$package_app" '*.pyo'
    validate_absent_pattern "$package_app" '*.pdb'
    validate_absent_pattern "$package_app" '*.bat'
    validate_absent_pattern "$package_app" '*.resources.dll'
    validate_absent_pattern "$package_app" '.DS_Store'

    for foreign_assembly in \
        Avalonia.FreeDesktop.AtSpi.dll \
        Avalonia.FreeDesktop.dll \
        Avalonia.Win32.Automation.dll \
        Avalonia.Win32.dll \
        Avalonia.X11.dll \
        NAudio.Asio.dll \
        NAudio.Midi.dll \
        NAudio.Wasapi.dll \
        NAudio.WinMM.dll \
        NAudio.dll \
        Tmds.DBus.Protocol.dll; do
        if [ -e "$package_macos/$foreign_assembly" ]; then
            echo "Foreign platform assembly was found: $package_macos/$foreign_assembly" >&2
            exit 1
        fi
    done

    for foreign_dependency in \
        Avalonia.FreeDesktop \
        Avalonia.FreeDesktop.AtSpi \
        Avalonia.Win32 \
        Avalonia.X11 \
        NAudio \
        NAudio.Asio \
        NAudio.Midi \
        NAudio.Wasapi \
        NAudio.WinMM \
        Tmds.DBus.Protocol; do
        if grep -F "\"$foreign_dependency\"" "$package_macos/Ludork.deps.json" >/dev/null \
            || grep -F "\"$foreign_dependency/" "$package_macos/Ludork.deps.json" >/dev/null; then
            echo "Foreign platform dependency was found in Ludork.deps.json: $foreign_dependency" >&2
            exit 1
        fi
    done

    if [ "$WITHOUT_TEMPLATES" -eq 1 ]; then
        return
    fi

    for template_name in $TEMPLATE_NAMES; do
        template_dir="$package_resources/Templates/$template_name"
        require_package_file "$template_dir/LICENSE.md"
        require_package_file "$template_dir/THIRD_PARTY_NOTICES.md"
        require_package_file "$template_dir/THIRD_PARTY_NOTICES_zh_CN.md"
        for template_licence_path in \
            README.md \
            README_zh_CN.md \
            Lua/LICENSE.txt \
            LuaSF/LICENSE.txt \
            SFML/LICENSE.txt \
            LuaGlue/LICENSE \
            lua-cjson/LICENSE.txt \
            zlib/LICENSE.txt \
            NativeDependencies/FLAC-COPYING.Xiph.txt \
            NativeDependencies/FreeType-FTL.txt \
            NativeDependencies/FreeType-LICENSE.txt \
            NativeDependencies/Glad-CC0-1.0.txt \
            NativeDependencies/HarfBuzz-COPYING.txt \
            NativeDependencies/libssh2-COPYING.txt \
            NativeDependencies/MbedTLS-LICENSE.txt \
            NativeDependencies/Ogg-COPYING.txt \
            NativeDependencies/SFML-THIRD-PARTY.md \
            NativeDependencies/SheenBidi-LICENSE.txt \
            NativeDependencies/Vorbis-COPYING.txt \
            NativeDependencies/Wine-DInput-LGPLv2.1.txt; do
            require_package_file "$template_dir/Licenses/$template_licence_path"
        done
        require_package_file "$template_dir/Assets/Fonts/LICENSE.txt"
        require_package_file "$template_dir/Assets/Musics/LICENSE.md"
        for excluded_licence_directory in \
            Avalonia \
            DotNet \
            DotNetPackages \
            EditorPackages \
            GNUMake \
            HarmonyOSSans \
            MicrosoftVisualCppRuntime \
            SampleMusic \
            ScriptTools; do
            if [ -e "$template_dir/Licenses/$excluded_licence_directory" ]; then
                echo "Non-runtime licence directory was found in a project template: $template_dir/Licenses/$excluded_licence_directory" >&2
                exit 1
            fi
        done
        for runtime_path in \
            "$template_dir/Log" \
            "$template_dir/Save" \
            "$template_dir/Main.ini" \
            "$template_dir/Ludork.ini" \
            "$template_dir/Ludork-startup-error.log"; do
            if [ -e "$runtime_path" ]; then
                echo "Template runtime state was found: $runtime_path" >&2
                exit 1
            fi
        done
    done

    for template_name in $PLAIN_TEMPLATE_NAMES; do
        template_licence_dir="$package_resources/Templates/$template_name/Licenses/FFmpeg"
        if [ -e "$template_licence_dir" ]; then
            echo "FFmpeg licence material was found in a non-FFmpeg template: $template_licence_dir" >&2
            exit 1
        fi
    done

    for template_name in $CPP_TEMPLATE_NAMES; do
        template_dir="$package_resources/Templates/$template_name"
        require_package_executable "$template_dir/generate_clion.sh"
        for foreign_ide_tool in \
            "$template_dir/generate_vs2022.bat" \
            "$template_dir/generate_clion.bat"; do
            if [ -e "$foreign_ide_tool" ]; then
                echo "Non-macOS IDE tool was found: $foreign_ide_tool" >&2
                exit 1
            fi
        done
        for generated_ide_entry in \
            "$template_dir/.vs" \
            "$template_dir/.idea" \
            "$template_dir/cmake-build-ludork-debug" \
            "$template_dir/CMakeUserPresets.json"; do
            if [ -e "$generated_ide_entry" ]; then
                echo "Generated IDE entry was found in a source template: $generated_ide_entry" >&2
                exit 1
            fi
        done
    done
    for template_name in $STANDALONE_TEMPLATE_NAMES; do
        template_dir="$package_resources/Templates/$template_name"
        for source_ide_tool in \
            "$template_dir/generate_vs2022.bat" \
            "$template_dir/generate_clion.bat" \
            "$template_dir/generate_clion.sh"; do
            if [ -e "$source_ide_tool" ]; then
                echo "Source-only IDE tool was found in a Standalone template: $source_ide_tool" >&2
                exit 1
            fi
        done
    done

    for template_name in $FFMPEG_TEMPLATE_NAMES; do
        template_licence_dir="$package_resources/Templates/$template_name/Licenses/FFmpeg"
        for template_licence_name in \
            COPYING.GPLv2.txt \
            COPYING.GPLv3.txt \
            COPYING.LGPLv2.1.txt \
            COPYING.LGPLv3.txt \
            README.md \
            UPSTREAM-LICENSE.md; do
            require_package_file "$template_licence_dir/$template_licence_name"
        done
    done
}

require_file "$PROJECT_FILE"
if [ "$(uname -s)" != "Darwin" ]; then
    echo "macOS is required to package the editor DMG." >&2
    exit 1
fi
if [ "$(uname -m)" != "arm64" ]; then
    echo "Apple Silicon is required to package the editor app." >&2
    exit 1
fi

require_command dotnet
find_cmake >/dev/null
require_command rsync
require_command sips
require_command iconutil
require_command plutil
require_command file
require_command hdiutil
require_command cmp
require_command codesign
require_command DeRez
require_command GetFileInfo
require_command Rez
require_command SetFile
require_command osascript
require_command sleep
require_command strings
require_command sync
require_command xattr
require_file /usr/bin/lipo

if [ "$WITHOUT_TEMPLATES" -eq 0 ]; then
    require_file "$PROJECT_ROOT/tools/create_templates.sh"
fi
require_file "$PROJECT_ROOT/tools/build_ui_preview_host.sh"
require_file "$PROJECT_ROOT/tools/common.sh"
require_file "$PROJECT_ROOT/tools/editor_runtime/build_cpp.sh"
require_file "$PROJECT_ROOT/tools/build_standalone.sh"
require_file "$PROJECT_ROOT/tools/pack_project.sh"
require_file "$PROJECT_ROOT/tools/editor_runtime/pack_ios.sh"
require_file "$PROJECT_ROOT/tools/editor_runtime/pack_harmony.sh"
require_file "$PROJECT_ROOT/tools/editor_runtime/pack_android.sh"
require_file "$PROJECT_ROOT/tools/editor_runtime/unregister_editor.sh"
require_file "$DMG_ASSET_DIR/background.svg"
require_file "$DMG_ASSET_DIR/installer_icon.svg"
require_file "$DMG_ASSET_DIR/configure_layout.applescript"
require_file "$DMG_ASSET_DIR/install_official_plugins.command"
if [ ! -x "$DMG_ASSET_DIR/install_official_plugins.command" ]; then
    echo "The DMG official plug-in installer is not executable." >&2
    exit 1
fi
sh -n "$DMG_ASSET_DIR/install_official_plugins.command"

if [ -L "$WORK_DIR" ] || { [ -e "$WORK_DIR" ] && [ ! -d "$WORK_DIR" ]; }; then
    echo "The editor package work path is not a normal directory: $WORK_DIR" >&2
    exit 1
fi
if [ -e "$BACKUP_DIR" ] || [ -L "$BACKUP_DIR" ]; then
    echo "A preserved editor package backup requires manual recovery: $BACKUP_DIR" >&2
    exit 1
fi
if [ -e "$DMG_MOUNT_DIR" ] || [ -L "$DMG_MOUNT_DIR" ] \
    || [ -e "$DMG_READ_WRITE" ] || [ -L "$DMG_READ_WRITE" ]; then
    echo "An editor DMG temporary path already exists; remove it after checking for an active mount." >&2
    exit 1
fi
if hdiutil info | grep -Fq "$WORK_DIR" \
    || hdiutil info | grep -Fq '/tmp/ludork-editor-dmg-mount.'; then
    echo "An earlier editor DMG is still mounted; detach it before packaging again." >&2
    exit 1
fi

require_file "$SCRIPT_TOOLS"
require_file "$SCRIPT_TOOLS_VERSION_REPORT"
if [ -n "$PREBUILT_TEMPLATES_DIR" ]; then
    require_directory "$PREBUILT_TEMPLATES_DIR"
    PREBUILT_TEMPLATES_DIR=$(absolute_path "$PREBUILT_TEMPLATES_DIR")
    case "$PREBUILT_TEMPLATES_DIR" in
        "$WORK_DIR" | "$WORK_DIR"/*)
            echo "The prepared template folder must remain outside $WORK_DIR." >&2
            exit 1
            ;;
    esac
fi
if [ "$WITHOUT_TEMPLATES" -eq 0 ]; then
    require_file "$PROJECT_ROOT/Game/CMakeLists.txt"
    require_directory "$PROJECT_ROOT/Game/Engine/ThirdParty/LuaSF"
    require_directory "$PROJECT_ROOT/Game/Engine/ThirdParty/SFML"
    require_directory "$PROJECT_ROOT/Game/Engine/ThirdParty/LuaGlue"
    require_directory "$PROJECT_ROOT/Game/Engine/ThirdParty/Lua"
    require_directory "$PROJECT_ROOT/Game/Engine/ThirdParty/lua-cjson"
    require_directory "$PROJECT_ROOT/Game/Engine/ThirdParty/zlib"
    require_file "$PROJECT_ROOT/Game/Engine/ThirdParty/ffmpeg/configure"
    require_file "$FFMPEG_SOURCE_ARCHIVE"
fi
require_file "$PROJECT_ROOT/Locale/locale.json"
require_file "$PROJECT_ROOT/LICENSE.md"
require_file "$PROJECT_ROOT/README.md"
require_file "$PROJECT_ROOT/README_zh_CN.md"
require_file "$PROJECT_ROOT/docs/THIRD_PARTY_NOTICES.md"
require_file "$PROJECT_ROOT/docs/THIRD_PARTY_NOTICES_zh_CN.md"
require_file "$PROJECT_ROOT/docs/About_en_GB.md"
require_file "$PROJECT_ROOT/docs/About_zh_CN.md"
require_file "$PROJECT_ROOT/Editor/Assets/icon.ico"
require_file "$PROJECT_ROOT/Editor/Assets/project-icon.png"
require_directory "$PROJECT_ROOT/docs/_images"
require_directory "$PROJECT_ROOT/docs/en_GB"
require_directory "$PROJECT_ROOT/docs/zh_CN"
require_directory "$PROJECT_ROOT/Licenses"

if [ -d "$WORK_DIR" ]; then
    rm -rf "$WORK_DIR"
fi
WORK_DIR_OWNED=1
mkdir -p "$MACOS_DIR" "$RESOURCES_DIR" "$STAGE_DIR"

BUILD_INFO="$RESOURCES_DIR/BuildInfo.json"
FULL_VERSION=$("$SCRIPT_TOOLS" packaging-constants release-build-info "$RESOURCES_DIR" \
    --version "$PRODUCT_VERSION" "$PACKAGE_CHANNEL")
DMG_FILE_NAME="Ludork-$FULL_VERSION-macos-arm64.dmg"
DMG_VOLUME_NAME="Ludork $FULL_VERSION"
STAGE_DMG="$STAGE_DIR/$DMG_FILE_NAME"
echo "Packaging Ludork $FULL_VERSION for macOS Apple Silicon..."

echo "Publishing macOS Apple Silicon editor..."
set --
if [ "$USE_CURRENT_EDITOR_BUILD" -eq 1 ]; then
    dotnet restore "$PROJECT_FILE" -r osx-arm64 \
        -p:Configuration=Release -p:SelfContained=true \
        -p:PublishSingleFile=false -p:PublishTrimmed=false -p:PublishAot=false \
        -p:DebugSymbols=false -p:DebugType=None
    set -- --no-build
fi
dotnet publish "$PROJECT_FILE" \
    "$@" \
    -c Release \
    -r osx-arm64 \
    --self-contained true \
    -o "$MACOS_DIR" \
    -p:PublishSingleFile=false \
    -p:PublishTrimmed=false \
    -p:PublishAot=false \
    -p:DebugSymbols=false \
    -p:DebugType=None

"$SCRIPT_TOOLS" prune-editor-macos-publish "$MACOS_DIR"

echo "Compiling packaged locale data..."
(
    cd "$MACOS_DIR"
    "$MACOS_DIR/Ludork" --compile-locale
)
rm -f "$MACOS_DIR/Locale/locale.json"
copy_directory "$MACOS_DIR/Locale" "$RESOURCES_DIR/Locale"
rm -rf "$MACOS_DIR/Locale" "$MACOS_DIR/docs" "$MACOS_DIR/Page" "$MACOS_DIR/Licenses"
rm -f \
    "$MACOS_DIR"/About_*.md \
    "$MACOS_DIR/LICENSE.md" \
    "$MACOS_DIR/README.md" \
    "$MACOS_DIR/README_zh_CN.md" \
    "$MACOS_DIR/THIRD_PARTY_NOTICES.md" \
    "$MACOS_DIR/THIRD_PARTY_NOTICES_zh_CN.md"

if [ "$WITHOUT_TEMPLATES" -eq 0 ]; then
    if [ -n "$PREBUILT_TEMPLATES_DIR" ]; then
        echo "Copying prepared editor project templates..."
        copy_directory "$PREBUILT_TEMPLATES_DIR" "$RESOURCES_DIR/Templates"
    else
        echo "Generating editor project templates..."
        sh "$PROJECT_ROOT/tools/create_templates.sh" Release "$RESOURCES_DIR/Templates"
    fi
fi

echo "Copying editor resources..."
rm -rf "$RESOURCES_DIR/docs" "$RESOURCES_DIR/Page" "$RESOURCES_DIR/Licenses"
copy_public_docs "$RESOURCES_DIR/docs"
copy_directory "$PROJECT_ROOT/Licenses" "$RESOURCES_DIR/Licenses"
"$SCRIPT_TOOLS" legal-resources editor "$PROJECT_ROOT" "$RESOURCES_DIR"
copy_about_files "$RESOURCES_DIR/docs"

mkdir -p "$RESOURCES_DIR/tools"
cp "$PROJECT_ROOT/tools/common.sh" "$RESOURCES_DIR/tools/common.sh"
cp "$PROJECT_ROOT/tools/editor_runtime/build_cpp.sh" "$RESOURCES_DIR/tools/build_cpp.sh"
cp "$PROJECT_ROOT/tools/build_standalone.sh" "$RESOURCES_DIR/tools/build_standalone.sh"
cp "$PROJECT_ROOT/tools/build_ui_preview_host.sh" "$RESOURCES_DIR/tools/build_ui_preview_host.sh"
cp "$PROJECT_ROOT/tools/pack_project.sh" "$RESOURCES_DIR/tools/pack_project.sh"
cp "$PROJECT_ROOT/tools/editor_runtime/pack_ios.sh" "$RESOURCES_DIR/tools/pack_ios.sh"
cp "$PROJECT_ROOT/tools/editor_runtime/pack_harmony.sh" "$RESOURCES_DIR/tools/pack_harmony.sh"
cp "$PROJECT_ROOT/tools/editor_runtime/pack_android.sh" "$RESOURCES_DIR/tools/pack_android.sh"
cp "$PROJECT_ROOT/tools/editor_runtime/unregister_editor.sh" "$RESOURCES_DIR/tools/unregister_editor.sh"
require_file "$LUAC"
copy_directory "$SCRIPT_TOOLS_DIRECTORY" "$RESOURCES_DIR/tools/ScriptTools"
cp "$LUAC" "$RESOURCES_DIR/tools/luac"
chmod +x \
    "$RESOURCES_DIR/tools/build_cpp.sh" \
    "$RESOURCES_DIR/tools/build_standalone.sh" \
    "$RESOURCES_DIR/tools/build_ui_preview_host.sh" \
    "$RESOURCES_DIR/tools/pack_project.sh" \
    "$RESOURCES_DIR/tools/pack_ios.sh" \
    "$RESOURCES_DIR/tools/pack_harmony.sh" \
    "$RESOURCES_DIR/tools/pack_android.sh" \
    "$RESOURCES_DIR/tools/unregister_editor.sh" \
    "$RESOURCES_DIR/tools/luac"

"$SCRIPT_TOOLS" editor-macos-metadata generate \
    "$APP_DIR/Contents/Info.plist" \
    --build-info "$BUILD_INFO"
create_bundle_icons
purge_python_cache "$APP_DIR"
purge_debug_symbols "$APP_DIR"
purge_windows_tools "$APP_DIR"
if [ "$WITHOUT_TEMPLATES" -eq 0 ]; then
    purge_template_runtime_state "$RESOURCES_DIR/Templates"
fi
find "$APP_DIR" -name '.DS_Store' -delete

if [ -n "$SIGNING_IDENTITY$SIGNING_CERTIFICATE" ]; then
    SIGNING_ENTITLEMENTS=${SIGNING_ENTITLEMENTS:-$DMG_ASSET_DIR/Editor.entitlements}
    echo "Signing the editor application..."
    run_macos_sign app "$APP_DIR"
fi

echo "Preparing official editor plugins..."
"$SCRIPT_TOOLS" editor-official-plugins prepare \
    "$PROJECT_ROOT/Plugins" \
    "$DMG_PAYLOAD_ROOT"
ln -s /Applications "$DMG_ROOT/Applications"
create_dmg_assets
validate_dmg_root "$DMG_ROOT" 0

echo "Creating macOS editor DMG..."
DMG_TEMP_OWNED=1
hdiutil create \
    -srcfolder "$DMG_ROOT" \
    -volname "$DMG_VOLUME_NAME" \
    -fs HFS+ \
    -format UDRW \
    -nospotlight \
    -ov \
    "$DMG_READ_WRITE" >/dev/null
mkdir "$DMG_MOUNT_DIR"
DMG_MOUNT_DIR_OWNED=1
attach_dmg "$DMG_READ_WRITE" -readwrite
if ! osascript "$DMG_ASSET_DIR/configure_layout.applescript" "$DMG_MOUNT_DIR"; then
    sleep 2
    osascript "$DMG_ASSET_DIR/configure_layout.applescript" "$DMG_MOUNT_DIR"
fi
cp "$DMG_ROOT/.VolumeIcon.icns" "$DMG_MOUNT_DIR/.VolumeIcon.icns"
xattr -c "$DMG_MOUNT_DIR/.VolumeIcon.icns"
SetFile -c icnC "$DMG_MOUNT_DIR/.VolumeIcon.icns"
SetFile -a V "$DMG_MOUNT_DIR/.VolumeIcon.icns"
SetFile -a C "$DMG_MOUNT_DIR"
sync
validate_dmg_root "$DMG_MOUNT_DIR" 1
detach_dmg
rmdir "$DMG_MOUNT_DIR"
DMG_MOUNT_DIR_OWNED=0
hdiutil convert \
    "$DMG_READ_WRITE" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -ov \
    -o "$STAGE_DMG" >/dev/null
unlink "$DMG_READ_WRITE"
DMG_TEMP_OWNED=0
validate_dmg "$STAGE_DMG"

if [ "$SIGNING_NOTARIZE" -eq 1 ]; then
    echo "Signing and notarising the editor disk image..."
    run_macos_sign dmg-notarize "$STAGE_DMG"
elif [ -n "$SIGNING_IDENTITY$SIGNING_CERTIFICATE" ]; then
    echo "Signing the editor disk image..."
    run_macos_sign dmg "$STAGE_DMG"
fi

if [ -e "$FINAL_DIR" ] || [ -L "$FINAL_DIR" ]; then
    mv "$FINAL_DIR" "$BACKUP_DIR"
    DIST_BACKED_UP=1
fi
mv "$STAGE_DIR" "$FINAL_DIR"
DIST_BACKED_UP=0
if [ -e "$BACKUP_DIR" ]; then
    rm -rf "$BACKUP_DIR"
fi
if [ -d "$WORK_DIR" ]; then
    rm -rf "$WORK_DIR"
fi
WORK_DIR_OWNED=0

echo "Editor package complete: $FINAL_DIR/$DMG_FILE_NAME"
