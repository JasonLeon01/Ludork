#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/common.sh"

download_dependency() {
    curl --location --fail --show-error \
        --retry 3 --retry-connrefused --retry-delay 5 --retry-max-time 900 \
        --connect-timeout 15 --max-time 300 \
        "$1" --output "$2"
}

dependency_ready() {
    dependency_dir=$1
    version=$2
    required_file=$3
    [ -f "$dependency_dir/.ludork-version" ] &&
        [ "$(cat "$dependency_dir/.ludork-version")" = "$version" ] &&
        [ -f "$dependency_dir/$required_file" ]
}
