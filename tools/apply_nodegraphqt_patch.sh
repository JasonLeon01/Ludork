#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PATCH_FILE="$ROOT_DIR/patches/NodeGraphQt-patch-issue-411.diff"
TARGET_DIR="$ROOT_DIR/LudorkEnv/lib/python3.12/site-packages/NodeGraphQt"

if [ ! -f "$TARGET_DIR/qgraphics/node_base.py" ]; then
  echo "NodeGraphQt is not installed in LudorkEnv, skipping patch."
  exit 0
fi

if [ ! -f "$PATCH_FILE" ]; then
  echo "Patch file not found: $PATCH_FILE"
  exit 1
fi

if ! command -v git >/dev/null 2>&1; then
  echo "git is required to apply NodeGraphQt patches."
  exit 1
fi

PATCH_WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/ludork-nodegraphqt-patch.XXXXXX")"
cleanup() {
  rm -rf "$PATCH_WORKDIR"
  unset GIT_DIR
}
trap cleanup EXIT

pushd "$PATCH_WORKDIR" >/dev/null
git init -q
export GIT_DIR="$PATCH_WORKDIR/.git"
pushd "$TARGET_DIR" >/dev/null

if git apply --reverse --check "$PATCH_FILE" >/dev/null 2>&1; then
  echo "NodeGraphQt patch issue-411 is already applied."
  popd >/dev/null
  popd >/dev/null
  exit 0
fi

if ! git apply --check "$PATCH_FILE"; then
  echo "Failed to validate NodeGraphQt patch issue-411. Installed NodeGraphQt version may differ from the patch target (0.6.44)."
  popd >/dev/null
  popd >/dev/null
  exit 1
fi

git apply "$PATCH_FILE"
popd >/dev/null
popd >/dev/null
echo "Applied NodeGraphQt patch issue-411."
