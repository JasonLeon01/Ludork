#!/usr/bin/env sh
set -eu

executable_dir=$(CDPATH= cd -- "$1" && pwd)
version=$2
icon_source=$3
intermediate_dir=$4
script_tools=$5
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

case "$executable_dir" in
    */Ludork.app/Contents/MacOS) ;;
    *)
        echo "Unexpected macOS development bundle path: $executable_dir" >&2
        exit 1
        ;;
esac

contents_dir=$(dirname -- "$executable_dir")
bundle_dir=$(dirname -- "$contents_dir")
resources_dir="$contents_dir/Resources"
mkdir -p "$resources_dir"
cp "$source_dir/Info.plist" "$contents_dir/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $version" "$contents_dir/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $version" "$contents_dir/Info.plist"

if [ ! -f "$resources_dir/AppIcon.icns" ] || [ "$icon_source" -nt "$resources_dir/AppIcon.icns" ]; then
    iconset_dir="$intermediate_dir/LudorkDevelopment.iconset"
    mkdir -p "$iconset_dir"
    for size in 16 32 128 256 512; do
        sips -z "$size" "$size" "$icon_source" --out "$iconset_dir/icon_${size}x${size}.png" >/dev/null
        double_size=$((size * 2))
        sips -z "$double_size" "$double_size" "$icon_source" --out "$iconset_dir/icon_${size}x${size}@2x.png" >/dev/null
    done
    iconutil -c icns "$iconset_dir" -o "$resources_dir/AppIcon.icns"
    rm -rf "$iconset_dir"
fi

find "$executable_dir" -type f -links +1 -exec sh -eu -c '
    for linked_file do
        signing_copy=$(mktemp "$linked_file.signing.XXXXXX")
        if ! cp -p "$linked_file" "$signing_copy"; then
            rm -f "$signing_copy"
            exit 1
        fi
        mv -f "$signing_copy" "$linked_file"
    done
' sh {} +

"$script_tools" macos-sign --ignore-environment --signing-identity - "$bundle_dir"
