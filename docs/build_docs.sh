#!/usr/bin/env sh
set -eu

DOCS_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DOCS_DIR"

rm -f index.html
rm -rf assets

cd "$DOCS_DIR/__default__"
npm run build

if [ ! -d dist ]; then
    echo "docs/__default__/dist was not produced." >&2
    exit 1
fi

cp -R dist/. "$DOCS_DIR/"
echo "Copied docs/__default__/dist into $DOCS_DIR"
