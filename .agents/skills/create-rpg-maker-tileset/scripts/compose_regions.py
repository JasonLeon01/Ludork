#!/usr/bin/env python3
"""Copy and repeat original-size image regions without resampling pixels."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

from PIL import Image, ImageDraw


def positive_int(value: object, label: str) -> int:
    if type(value) is not int or value <= 0:
        raise ValueError(f"{label} must be a positive integer")
    return value


def rectangle(value: object, label: str) -> tuple[int, int, int, int]:
    if not isinstance(value, list) or len(value) != 4 or any(type(v) is not int for v in value):
        raise ValueError(f"{label} must be [x, y, width, height] in integer pixels")
    x, y, width, height = value
    if x < 0 or y < 0 or width <= 0 or height <= 0:
        raise ValueError(f"{label} has invalid origin or size")
    return x, y, width, height


def box(rect: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    x, y, width, height = rect
    return x, y, x + width, y + height


def place(canvas: Image.Image, patch: Image.Image, rect: tuple[int, int, int, int]) -> None:
    x, y, width, height = rect
    for dy in range(0, height, patch.height):
        for dx in range(0, width, patch.width):
            part = patch.crop((0, 0, min(patch.width, width - dx), min(patch.height, height - dy)))
            canvas.paste(part, (x + dx, y + dy))


def compose(manifest_path: Path, include_mapping: bool = False) -> tuple[Image.Image, dict | None]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("manifest root must be an object")
    tile = positive_int(manifest.get("tile_size", 32), "tile_size")
    if tile != 32:
        raise ValueError("tile_size must be 32")
    width = positive_int(manifest.get("width"), "width")
    height = positive_int(manifest.get("height"), "height")
    if width % tile or height % tile:
        raise ValueError("canvas dimensions must be multiples of tile_size")
    operations = manifest.get("operations")
    if not isinstance(operations, list) or not operations:
        raise ValueError("operations must be a non-empty array")

    canvas = Image.new("RGBA", (width, height))
    owners = Image.new("I", canvas.size, -1) if include_mapping else None
    sources: dict[Path, tuple[Image.Image, Image.Image | None]] = {}
    source_tiles: list[dict] = []
    for index, operation in enumerate(operations):
        if not isinstance(operation, dict) or operation.get("op") not in ("copy", "repeat"):
            raise ValueError(f"operations[{index}].op must be copy or repeat")
        source = operation.get("source")
        if not isinstance(source, str) or not source:
            raise ValueError(f"operations[{index}].source must be a file path")
        path = (manifest_path.parent / source).resolve()
        if path not in sources:
            with Image.open(path) as opened:
                if opened.format != "PNG" or opened.mode != "RGBA":
                    raise ValueError(f"{source}: source must be RGBA PNG")
                image = opened.copy()
            labels = None
            if include_mapping:
                labels = Image.new("I", image.size, -1)
                draw = ImageDraw.Draw(labels)
                columns = (image.width + tile - 1) // tile
                for sy in range(0, image.height, tile):
                    for sx in range(0, image.width, tile):
                        label = len(source_tiles)
                        source_tiles.append({"source": source, "source_tile": sy // tile * columns + sx // tile})
                        draw.rectangle((sx, sy, min(sx + tile, image.width) - 1, min(sy + tile, image.height) - 1), fill=label)
            sources[path] = image, labels
        image, labels = sources[path]
        source_rect = rectangle(operation.get("source_rect"), f"operations[{index}].source_rect")
        target_rect = rectangle(operation.get("dest_rect"), f"operations[{index}].dest_rect")
        if box(source_rect)[2] > image.width or box(source_rect)[3] > image.height:
            raise ValueError(f"operations[{index}]: source rectangle exceeds image")
        if box(target_rect)[2] > width or box(target_rect)[3] > height:
            raise ValueError(f"operations[{index}]: destination rectangle exceeds canvas")
        if operation["op"] == "copy" and source_rect[2:] != target_rect[2:]:
            raise ValueError(f"operations[{index}]: copy cannot resize a region; use repeat for added area")
        place(canvas, image.crop(box(source_rect)), target_rect)
        if owners is not None and labels is not None:
            place(owners, labels.crop(box(source_rect)), target_rect)

    mapping = None
    if owners is not None:
        cells = []
        alpha = canvas.getchannel("A")
        for y in range(0, height, tile):
            for x in range(0, width, tile):
                region = (x, y, x + tile, y + tile)
                labels = list(owners.crop(region).getdata())
                opacity = list(alpha.crop(region).getdata())
                counts = Counter(label for label, a in zip(labels, opacity) if label >= 0 and a > 0)
                if not counts:
                    counts = Counter(label for label in labels if label >= 0)
                counts = sorted(counts.items(), key=lambda pair: (-pair[1], pair[0]))
                primary = source_tiles[counts[0][0]] if counts else None
                cells.append({"target_tile": y // tile * (width // tile) + x // tile, "primary": primary,
                              "sources": [dict(source_tiles[label], pixels=count) for label, count in counts]})
        mapping = {"tile_size": tile, "columns": width // tile, "rows": height // tile, "cells": cells}
    return canvas, mapping


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--mapping", type=Path, help="Optional per-cell source provenance JSON")
    args = parser.parse_args()
    if args.mapping and args.mapping.resolve() == args.output.resolve():
        raise ValueError("image and mapping destinations must differ")
    canvas, mapping = compose(args.manifest.resolve(), args.mapping is not None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(args.output, format="PNG")
    if args.mapping:
        args.mapping.parent.mkdir(parents=True, exist_ok=True)
        args.mapping.write_text(json.dumps(mapping, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({canvas.width}x{canvas.height}, RGBA; no resampling)")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        raise SystemExit(f"error: {exc}") from exc
