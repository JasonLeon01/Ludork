#!/usr/bin/env sh
set -eu

DOCS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DOCS_DIR/__default__"
npm run build

for entry in index.html docs/index.html about/index.html notices/index.html favicon.svg .nojekyll; do
    if [ ! -f "dist/$entry" ]; then
        echo "Missing website output: $entry" >&2
        exit 1
    fi
done
if [ ! -d dist/assets ]; then
    echo "Website assets were not produced." >&2
    exit 1
fi

rm -f "$DOCS_DIR/index.html"
rm -rf "$DOCS_DIR/assets" "$DOCS_DIR/docs" "$DOCS_DIR/about" "$DOCS_DIR/notices"
cp -R dist/. "$DOCS_DIR/"
echo "Copied docs/__default__/dist into $DOCS_DIR"
