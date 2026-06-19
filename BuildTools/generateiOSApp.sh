#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# generate_ios_app.sh
#
# Generates a complete Xcode iOS project from the iOSTemplate.
#
# Usage:
#   ./generateiOSApp.sh <project_name> [-g|--game-root <game_project_root>] [scripts_dir] [ios_python_dir] [-r icon_dir]
#
#   -g <dir>  Game project root (the opened game repo). Output is written to:
#             <game_project_root>/build/<project_name>/
#             If omitted, uses env LUDORK_GAME_ROOT, otherwise the current working directory.
#             The editor passes -g with EditorStatus.PROJ_PATH (see BuildTools/pack_ios.sh).
#
#   -r <dir>  Icon directory. Searches in priority:
#             1. icon.icns  ->  converted to PNG via sips
#             2. icon.ico   ->  converted to PNG via sips
#             3. icon.png   ->  used directly
#
# Output:
#   <game_project_root>/build/<project_name>/
#     +-- <project_name>.xcodeproj/
#     +-- <project_name>/          (source + icon)
#
# Requirements: macOS (sips for icon conversion), no CWD dependency.
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LUDORK_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

fail() { echo "Error: $*" >&2; exit 1; }

PROJECT_NAME=""
GAME_ROOT=""
SCRIPTS_DIR=""
IOS_PYTHON_DIR=""
RESOURCE_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -g|--game-root)
            GAME_ROOT="$2"
            shift 2
            ;;
        -r|--resource)
            RESOURCE_DIR="$2"
            shift 2
            ;;
        -*)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
        *)
            if [ -z "$PROJECT_NAME" ]; then
                PROJECT_NAME="$1"
            elif [ -z "$SCRIPTS_DIR" ]; then
                SCRIPTS_DIR="$1"
            elif [ -z "$IOS_PYTHON_DIR" ]; then
                IOS_PYTHON_DIR="$1"
            elif [ -z "$RESOURCE_DIR" ]; then
                RESOURCE_DIR="$1"
            else
                echo "Unexpected argument: $1" >&2
                exit 1
            fi
            shift
            ;;
    esac
done

PROJECT_NAME="${PROJECT_NAME:?Usage: $0 <project_name> [-g|--game-root <game_project_root>] [scripts_dir] [ios_python_dir] [-r icon_dir]}"
GAME_ROOT="${GAME_ROOT:-${LUDORK_GAME_ROOT:-}}"
if [ -z "${GAME_ROOT}" ]; then
    GAME_ROOT="$(pwd -P)"
fi
GAME_ROOT="$(cd "${GAME_ROOT}" && pwd -P)" || fail "Game project root is not a directory: ${GAME_ROOT}"
[ -d "${GAME_ROOT}" ] || fail "Game project root not found: ${GAME_ROOT}"

SCRIPTS_DIR="${SCRIPTS_DIR:-${LUDORK_ROOT}/Sample}"
IOS_PYTHON_DIR="${IOS_PYTHON_DIR:-${LUDORK_ROOT}/ios_python}"
C_EXT_BUILD_DIR="${LUDORK_ROOT}/C_Extensions/build_ios"
RESOURCE_DIR="${RESOURCE_DIR:-${LUDORK_ROOT}/resources}"

TEMPLATE_DIR="${LUDORK_ROOT}/iOSTemplate"
OUTPUT_DIR="${GAME_ROOT}/build/${PROJECT_NAME}"

[ -d "${SCRIPTS_DIR}" ]       || fail "scripts directory not found: ${SCRIPTS_DIR}"
[ -d "${IOS_PYTHON_DIR}" ]    || fail "ios_python directory not found: ${IOS_PYTHON_DIR}"
[ -d "${C_EXT_BUILD_DIR}" ]   || fail "C_Extensions/build_ios not found: ${C_EXT_BUILD_DIR}"
[ -f "${SCRIPTS_DIR}/Entry.py" ] || fail "Entry.py not found in: ${SCRIPTS_DIR}"
[ -d "${TEMPLATE_DIR}" ]      || fail "template directory not found: ${TEMPLATE_DIR}"
[ -f "${TEMPLATE_DIR}/project.pbxproj.template" ] || fail "pbxproj template missing"

# --- Build Python.xcframework from static libraries if missing ---
XCFRAMEWORK_PATH="${IOS_PYTHON_DIR}/Python.xcframework"
LIB_DEVICE="${IOS_PYTHON_DIR}/ios-arm64/libPython3.12.a"
HEADERS_DEVICE="${IOS_PYTHON_DIR}/ios-arm64/Headers"
LIB_SIM="${IOS_PYTHON_DIR}/ios-arm64_x86_64-simulator/libPython3.12.a"
HEADERS_SIM="${IOS_PYTHON_DIR}/ios-arm64_x86_64-simulator/Headers"

if [ ! -d "${XCFRAMEWORK_PATH}" ]; then
    [ -f "${LIB_DEVICE}" ]   || fail "Device static library not found: ${LIB_DEVICE}"
    [ -d "${HEADERS_DEVICE}" ] || fail "Device headers not found: ${HEADERS_DEVICE}"
    [ -f "${LIB_SIM}" ]      || fail "Simulator static library not found: ${LIB_SIM}"
    [ -d "${HEADERS_SIM}" ]  || fail "Simulator headers not found: ${HEADERS_SIM}"

    echo "Creating Python.xcframework (manual)..."

    mkdir -p "${XCFRAMEWORK_PATH}/ios-arm64"
    mkdir -p "${XCFRAMEWORK_PATH}/ios-arm64_x86_64-simulator"

    cp "${LIB_DEVICE}"   "${XCFRAMEWORK_PATH}/ios-arm64/"
    cp -R "${HEADERS_DEVICE}" "${XCFRAMEWORK_PATH}/ios-arm64/"
    cp "${LIB_SIM}"      "${XCFRAMEWORK_PATH}/ios-arm64_x86_64-simulator/"
    cp -R "${HEADERS_SIM}"    "${XCFRAMEWORK_PATH}/ios-arm64_x86_64-simulator/"

    cat > "${XCFRAMEWORK_PATH}/Info.plist" << 'PLISTEOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>AvailableLibraries</key>
    <array>
        <dict>
            <key>LibraryIdentifier</key>
            <string>ios-arm64</string>
            <key>LibraryPath</key>
            <string>libPython3.12.a</string>
            <key>SupportedArchitectures</key>
            <array>
                <string>arm64</string>
            </array>
            <key>SupportedPlatform</key>
            <string>ios</string>
        </dict>
        <dict>
            <key>LibraryIdentifier</key>
            <string>ios-arm64_x86_64-simulator</string>
            <key>LibraryPath</key>
            <string>libPython3.12.a</string>
            <key>SupportedArchitectures</key>
            <array>
                <string>arm64</string>
                <string>x86_64</string>
            </array>
            <key>SupportedPlatform</key>
            <string>ios</string>
            <key>SupportedPlatformVariant</key>
            <string>simulator</string>
        </dict>
    </array>
    <key>CFBundlePackageType</key>
    <string>XFWK</string>
    <key>XCFrameworkFormatVersion</key>
    <string>1.0</string>
</dict>
</plist>
PLISTEOF

    echo "Python.xcframework created at ${XCFRAMEWORK_PATH}"
else
    echo "Python.xcframework already exists, skipping creation"
fi

echo "Generate iOS Project"
echo "  Project:       ${PROJECT_NAME}"
echo "  Game root:     ${GAME_ROOT}"
echo "  Scripts dir:   ${SCRIPTS_DIR}"
echo "  iOS Python:    ${IOS_PYTHON_DIR}"
echo "  Resources:     ${RESOURCE_DIR}"
echo "  Output:        ${OUTPUT_DIR}"

ICON_SOURCE=""

find_icon() {
    local dir="$1"
    if [ -f "${dir}/icon.icns" ]; then
        echo "${dir}/icon.icns"
        return
    fi
    if [ -f "${dir}/icon.ico" ]; then
        echo "${dir}/icon.ico"
        return
    fi
    if [ -f "${dir}/icon.png" ]; then
        echo "${dir}/icon.png"
        return
    fi
    echo ""
}

if [ -d "${RESOURCE_DIR}" ]; then
    ICON_SOURCE=$(find_icon "${RESOURCE_DIR}")
    if [ -n "${ICON_SOURCE}" ]; then
        echo "Icon found: ${ICON_SOURCE}"
    else
        echo "No icon in resource directory, using template default"
    fi
else
    echo "Resource directory not found, using template default icon"
fi

if [ -d "${OUTPUT_DIR}" ]; then
    echo "Cleaning old output directory..."
    rm -rf "${OUTPUT_DIR}"
fi

SOURCE_SUBDIR="${OUTPUT_DIR}/${PROJECT_NAME}"
XCODEPROJ_DIR="${OUTPUT_DIR}/${PROJECT_NAME}.xcodeproj"

mkdir -p "${SOURCE_SUBDIR}"
mkdir -p "${XCODEPROJ_DIR}"

echo "Copying source files..."
cp "${TEMPLATE_DIR}/main.m"           "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/AppDelegate.h"    "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/AppDelegate.m"    "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/ViewController.h" "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/ViewController.m" "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/SFMLMain.mm"      "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/PythonRunner.h"   "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/PythonRunner.mm"  "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/Info.plist"       "${SOURCE_SUBDIR}/"
cp "${TEMPLATE_DIR}/AppIcon.png"      "${SOURCE_SUBDIR}/"

if [ -n "${ICON_SOURCE}" ] && [ -f "${ICON_SOURCE}" ]; then
    EXT="${ICON_SOURCE##*.}"
    DST_ICON="${SOURCE_SUBDIR}/AppIcon.png"
    CONVERTED=0

    case "${EXT}" in
        png|PNG)
            cp "${ICON_SOURCE}" "${DST_ICON}"
            echo "Icon copied (PNG)"
            CONVERTED=1
            ;;
        icns|ICNS)
            echo "Converting icon.icns -> AppIcon.png..."
            rm -f "${DST_ICON}"
            sips -s format png "${ICON_SOURCE}" --out "${DST_ICON}" 2>/dev/null && CONVERTED=1 || true
            if [ "${CONVERTED}" -eq 0 ]; then
                TMP_ICONSET="/tmp/_appicon.iconset"
                rm -rf "${TMP_ICONSET}"
                mkdir -p "${TMP_ICONSET}"
                for size in 16 32 64 128 256 512; do
                    sips -z ${size} ${size} "${ICON_SOURCE}" --out "${TMP_ICONSET}/icon_${size}x${size}.png" 2>/dev/null || true
                done
                for size in 1024 512 256 128 64 32 16; do
                    if [ -f "${TMP_ICONSET}/icon_${size}x${size}.png" ]; then
                        cp "${TMP_ICONSET}/icon_${size}x${size}.png" "${DST_ICON}"
                        CONVERTED=1
                        break
                    fi
                done
                rm -rf "${TMP_ICONSET}"
            fi
            if [ "${CONVERTED}" -eq 1 ]; then
                echo "Icon converted"
            else
                echo "Warning: .icns conversion failed, using template default"
            fi
            ;;
        ico|ICO)
            echo "Converting icon.ico -> AppIcon.png..."
            rm -f "${DST_ICON}"
            sips -s format png "${ICON_SOURCE}" --out "${DST_ICON}" 2>/dev/null && CONVERTED=1 || true
            if [ "${CONVERTED}" -eq 1 ]; then
                echo "Icon converted"
            else
                echo "Warning: .ico conversion failed (sips may not support .ico), use PNG or icns"
            fi
            ;;
        *)
            echo "Warning: unsupported icon format: .${EXT}"
            ;;
    esac

    if [ "${CONVERTED}" -eq 0 ] && [ ! -f "${DST_ICON}" ]; then
        cp "${TEMPLATE_DIR}/AppIcon.png" "${DST_ICON}"
    fi
fi

echo "Replacing __PROJECT_NAME__ -> ${PROJECT_NAME}"
for f in "${SOURCE_SUBDIR}/PythonRunner.mm" "${SOURCE_SUBDIR}/Info.plist"; do
    if grep -q "__PROJECT_NAME__" "${f}" 2>/dev/null; then
        sed -i '' "s/__PROJECT_NAME__/${PROJECT_NAME}/g" "${f}"
    fi
done

echo "Generating project.pbxproj..."
sed "s/__PROJECT_NAME__/${PROJECT_NAME}/g" \
    "${TEMPLATE_DIR}/project.pbxproj.template" \
    > "${XCODEPROJ_DIR}/project.pbxproj"

compute_relpath() {
    python3 -c "import os,sys; print(os.path.relpath(sys.argv[1], sys.argv[2]))" "$1" "$OUTPUT_DIR"
}

REL_SCRIPTS=$(compute_relpath "${SCRIPTS_DIR}")
REL_IOS_PYTHON=$(compute_relpath "${IOS_PYTHON_DIR}")
REL_C_EXT_BUILD=$(compute_relpath "${C_EXT_BUILD_DIR}")
echo "Scripts path relative to generated project (SRCROOT): ${REL_SCRIPTS}"
echo "ios_python path relative to generated project (SRCROOT): ${REL_IOS_PYTHON}"
echo "C_Extensions/build_ios path relative to generated project (SRCROOT): ${REL_C_EXT_BUILD}"

sed -i '' "s|\.\./scripts/|${REL_SCRIPTS}/|g" "${XCODEPROJ_DIR}/project.pbxproj"
sed -i '' "s|path = \.\./scripts;|path = ${REL_SCRIPTS};|g" "${XCODEPROJ_DIR}/project.pbxproj"
sed -i '' "s|\$(SRCROOT)/\.\./scripts/|\$(SRCROOT)/${REL_SCRIPTS}/|g" "${XCODEPROJ_DIR}/project.pbxproj"

sed -i '' "s|\.\./ios_python/|${REL_IOS_PYTHON}/|g" "${XCODEPROJ_DIR}/project.pbxproj"
sed -i '' "s|\$(SRCROOT)/\.\./ios_python/|\$(SRCROOT)/${REL_IOS_PYTHON}/|g" "${XCODEPROJ_DIR}/project.pbxproj"

sed -i '' "s#__SCRIPTS_RSYNC_PATH__#${REL_SCRIPTS}#g" "${XCODEPROJ_DIR}/project.pbxproj"
sed -i '' "s#__IOS_PYTHON_REL__#${REL_IOS_PYTHON}#g" "${XCODEPROJ_DIR}/project.pbxproj"
sed -i '' "s#__C_EXT_BUILD__#${REL_C_EXT_BUILD}#g" "${XCODEPROJ_DIR}/project.pbxproj"

if [ -d "${TEMPLATE_DIR}/iOSTemplate.xcodeproj/project.xcworkspace" ]; then
    cp -R "${TEMPLATE_DIR}/iOSTemplate.xcodeproj/project.xcworkspace" "${XCODEPROJ_DIR}/"
fi
if [ -d "${TEMPLATE_DIR}/iOSTemplate.xcodeproj/xcshareddata" ]; then
    cp -R "${TEMPLATE_DIR}/iOSTemplate.xcodeproj/xcshareddata" "${XCODEPROJ_DIR}/"
    SCHEME_SRC="${XCODEPROJ_DIR}/xcshareddata/xcschemes/__PROJECT_NAME__.xcscheme"
    SCHEME_DST="${XCODEPROJ_DIR}/xcshareddata/xcschemes/${PROJECT_NAME}.xcscheme"
    if [ -f "${SCHEME_SRC}" ]; then
        mv "${SCHEME_SRC}" "${SCHEME_DST}"
        sed -i '' "s/__PROJECT_NAME__/${PROJECT_NAME}/g" "${SCHEME_DST}"
    fi
fi

echo ""
echo "Project generation completed"
echo ""
echo "  ${OUTPUT_DIR}/"
echo "  +-- ${PROJECT_NAME}.xcodeproj/"
echo "  +-- ${PROJECT_NAME}/"
if [ -n "${ICON_SOURCE}" ]; then
    echo "       +-- AppIcon.png  <- $(basename "${ICON_SOURCE}")"
fi
echo ""
echo "Build command:"
echo "  cd ${OUTPUT_DIR}"
echo "  xcodebuild -project ${PROJECT_NAME}.xcodeproj \\"
echo "    -scheme ${PROJECT_NAME} \\"
echo "    -sdk iphoneos \\"
echo "    -destination 'generic/platform=iOS' \\"
echo "    CODE_SIGNING_ALLOWED=NO build"
