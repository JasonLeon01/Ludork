#!/usr/bin/env python3
"""Validate PNG dimensions, alpha, reserved cell, and grid occupancy."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--profile", choices=("rmxp", "generic32"), default="rmxp")
    parser.add_argument("--tile-size", type=int, default=32)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    errors: list[str] = []
    warnings: list[str] = []

    try:
        with Image.open(args.image) as opened:
            source_format = opened.format
            source_mode = opened.mode
            image = opened.convert("RGBA")
    except OSError as exc:
        raise SystemExit(f"error: cannot open {args.image}: {exc}")

    tile = args.tile_size
    if tile <= 0:
        raise SystemExit("error: --tile-size must be positive")
    if args.profile == "rmxp" and tile != 32:
        errors.append("RMXP profile requires a 32px tile size")
    if image.width % tile or image.height % tile:
        errors.append(f"dimensions {image.width}x{image.height} are not multiples of {tile}")
    if args.profile == "rmxp" and image.width != 256:
        errors.append(f"RMXP tileset width must be 256px, got {image.width}px")
    if source_format != "PNG":
        errors.append(f"source format must be PNG, got {source_format!r}")
    if image.height <= 0:
        errors.append("image height must be positive")
    if "A" not in source_mode and source_mode not in ("LA", "PA"):
        errors.append(f"source mode {source_mode!r} has no alpha channel")

    alpha = image.getchannel("A")
    if alpha.getextrema() == (255, 255):
        warnings.append("image contains no transparent pixels")

    if image.width >= tile and image.height >= tile:
        reserved = alpha.crop((0, 0, tile, tile))
        if args.profile == "rmxp" and reserved.getbbox() is not None:
            errors.append("reserved cell (0,0) is not fully transparent")

    histogram = alpha.histogram()
    partial_alpha = sum(histogram[1:255])
    if partial_alpha:
        warnings.append(f"{partial_alpha} pixels use partial alpha; inspect for matte halos")

    empty_cells = 0
    if image.width % tile == 0 and image.height % tile == 0:
        columns = image.width // tile
        rows = image.height // tile
        for y in range(rows):
            for x in range(columns):
                box = (x * tile, y * tile, (x + 1) * tile, (y + 1) * tile)
                if alpha.crop(box).getbbox() is None:
                    empty_cells += 1
        if empty_cells == columns * rows:
            errors.append("all cells are transparent")

    print(f"file: {args.image}")
    print(f"size: {image.width}x{image.height}; source mode: {source_mode}; tile: {tile}px")
    print(f"empty cells: {empty_cells}")
    for warning in warnings:
        print(f"warning: {warning}")
    for error in errors:
        print(f"error: {error}")

    if errors:
        raise SystemExit(1)
    print("validation: passed")


if __name__ == "__main__":
    main()
