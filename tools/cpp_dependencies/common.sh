#!/usr/bin/env sh
set -eu

. "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)/common.sh"

dependency_ready() {
    dependency_dir=$1
    version=$2
    required_file=$3
    [ -f "$dependency_dir/.ludork-version" ] &&
        [ "$(cat "$dependency_dir/.ludork-version")" = "$version" ] &&
        [ -f "$dependency_dir/$required_file" ]
}
