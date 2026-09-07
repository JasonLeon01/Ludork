#!/usr/bin/env python3
"""Pack grid-aligned RGBA PNG modules into an RMXP or generic 32px tileset."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops


def fail(message: str) -> None:
    raise SystemExit(f"error: {message}")


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot read manifest {path}: {exc}")
    if not isinstance(data, dict):
        fail("manifest root must be an object")
    return data


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--allow-overwrite",
        action="store_true",
        help="allow later items to replace occupied pixels",
    )
    return parser.parse_args()


def alpha_mask(alpha: Image.Image) -> Image.Image:
    """Return a mode-1 mask where every nonzero alpha value is occupied."""
    return alpha.point(lambda value: 255 if value > 0 else 0).convert("1")


def main() -> None:
    args = parse_args()
    manifest = load_manifest(args.manifest)
    profile = manifest.get("profile", "rmxp")
    tile_size = manifest.get("tile_size", 32)
    columns = manifest.get("columns", 8 if profile == "rmxp" else None)
    rows = manifest.get("rows")
    items = manifest.get("items", [])

    if profile not in ("rmxp", "generic32"):
        fail(f"unsupported profile: {profile!r}")
    if type(tile_size) is not int or tile_size != 32:
        fail(f"{profile} profile requires tile_size 32")
    if type(columns) is not int or columns <= 0:
        fail("columns must be a positive integer (required for generic32)")
    if profile == "rmxp" and columns != 8:
        fail("rmxp profile requires 8 columns")
    if type(rows) is not int or rows <= 0:
        fail("rows must be a positive integer")
    if not isinstance(items, list):
        fail("items must be an array")

    width = columns * tile_size
    height = rows * tile_size
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    occupied = Image.new("1", (width, height), 0)
    base_dir = args.manifest.resolve().parent

    for index, item in enumerate(items):
        if not isinstance(item, dict):
            fail(f"items[{index}] must be an object")
        source = item.get("source")
        x = item.get("x")
        y = item.get("y")
        if not isinstance(source, str) or not source:
            fail(f"items[{index}].source must be a non-empty string")
        if type(x) is not int or type(y) is not int or x < 0 or y < 0:
            fail(f"items[{index}] x and y must be non-negative integers")

        source_path = (base_dir / source).resolve()
        try:
            with Image.open(source_path) as opened:
                if opened.format != "PNG":
                    fail(f"{source}: source format must be PNG, got {opened.format!r}")
                if opened.mode != "RGBA":
                    fail(f"{source}: source mode must be RGBA, got {opened.mode!r}")
                image = opened.copy()
        except OSError as exc:
            fail(f"cannot open {source_path}: {exc}")

        if image.width % tile_size or image.height % tile_size:
            fail(
                f"{source}: dimensions {image.width}x{image.height} are not "
                f"multiples of {tile_size}"
            )

        px = x * tile_size
        py = y * tile_size
        if px + image.width > width or py + image.height > height:
            fail(f"{source}: placement exceeds {width}x{height} canvas")

        alpha = image.getchannel("A")
        existing = occupied.crop((px, py, px + image.width, py + image.height))
        source_occupied = alpha_mask(alpha)
        overlap = ImageChops.logical_and(existing, source_occupied)
        if overlap.getbbox() is not None and not args.allow_overwrite:
            fail(f"{source}: non-transparent pixels overlap an earlier item")

        canvas.alpha_composite(image, (px, py))
        occupied.paste(ImageChops.logical_or(existing, source_occupied), (px, py))

    if profile == "rmxp":
        reserved = canvas.crop((0, 0, tile_size, tile_size)).getchannel("A")
        if reserved.getbbox() is not None:
            fail("reserved cell (0,0) is not fully transparent")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(args.output, format="PNG", optimize=False)
    print(f"wrote {args.output} ({width}x{height}, RGBA)")


if __name__ == "__main__":
    main()
