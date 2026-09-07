#!/usr/bin/env python3
"""Validate RGBA VFX frames and build sprite-sheet and animation previews."""

from __future__ import annotations

import argparse
import json
import math
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit(f'Pillow is required: "{sys.executable}" -m pip install Pillow') from exc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate transparent PNG frames and export a sprite sheet, APNG, GIF, and manifest."
    )
    parser.add_argument("frames_dir", type=Path, help="Directory containing ordered PNG frames")
    parser.add_argument("--output-dir", type=Path, required=True, help="Destination directory")
    parser.add_argument("--expected-frames", type=int, required=True, help="Required consecutive frame count")
    parser.add_argument("--fps", type=float, default=12.0, help="Playback rate (default: 12)")
    parser.add_argument("--loop", action="store_true", help="Loop previews forever; default is one-shot")
    parser.add_argument("--columns", type=int, default=0, help="Sprite-sheet columns; 0 chooses a square layout")
    parser.add_argument(
        "--alpha-threshold",
        type=int,
        default=1,
        choices=range(0, 256),
        metavar="0..255",
        help="Alpha value above which a pixel counts as occupied",
    )
    parser.add_argument(
        "--allow-opaque",
        action="store_true",
        help="Export even if a source frame has no transparent pixels",
    )
    return parser.parse_args()


def alpha_bbox(image: Image.Image, threshold: int) -> tuple[int, int, int, int] | None:
    alpha = image.getchannel("A")
    if threshold > 0:
        alpha = alpha.point(lambda value: 255 if value > threshold else 0)
    return alpha.getbbox()


def edge_alpha_ratio(image: Image.Image, threshold: int) -> float:
    alpha = image.getchannel("A")
    width, height = image.size
    coords = [(x, 0) for x in range(width)]
    if height > 1:
        coords.extend((x, height - 1) for x in range(width))
    coords.extend((0, y) for y in range(1, height - 1))
    if width > 1:
        coords.extend((width - 1, y) for y in range(1, height - 1))
    if not coords:
        return 0.0
    occupied = sum(1 for xy in coords if alpha.getpixel(xy) > threshold)
    return occupied / len(coords)


def load_frames(paths: list[Path], allow_opaque: bool) -> tuple[list[Image.Image], list[str]]:
    frames: list[Image.Image] = []
    warnings: list[str] = []
    expected_size: tuple[int, int] | None = None

    for path in paths:
        with Image.open(path) as source:
            source.load()
            if source.format != "PNG":
                raise ValueError(f"{path.name}: source format must be PNG")
            if expected_size is None:
                expected_size = source.size
            elif source.size != expected_size:
                raise ValueError(f"Canvas mismatch: {path.name} is {source.size}, expected {expected_size}")

            has_alpha = source.mode in {"RGBA", "LA"} or "transparency" in source.info
            frame = source.convert("RGBA")
            if not has_alpha or frame.getchannel("A").getextrema()[0] == 255:
                message = f"{path.name}: no transparent pixels"
                if not allow_opaque:
                    raise ValueError(message + " (use --allow-opaque only when intentional)")
                warnings.append(message)
            frames.append(frame)

    return frames, warnings


def save_gif(frames: list[Image.Image], path: Path, duration_ms: int, loop: bool) -> None:
    # GIF is preview-only: binary transparency cannot preserve soft alpha edges.
    preview_frames: list[Image.Image] = []
    for frame in frames:
        flat = Image.new("RGBA", frame.size, (24, 24, 30, 255))
        flat.alpha_composite(frame)
        preview_frames.append(flat.convert("RGB"))
    preview_frames[0].save(
        path,
        save_all=True,
        append_images=preview_frames[1:],
        duration=duration_ms,
        loop=0 if loop else None,
        disposal=2,
    )


def png_names(directory: Path) -> set[str]:
    return {path.name for path in directory.glob("*") if path.is_file() and path.suffix.lower() == ".png"}


def main() -> int:
    args = parse_args()
    if not math.isfinite(args.fps) or args.fps <= 0:
        raise ValueError("--fps must be finite and greater than zero")
    if args.expected_frames <= 0:
        raise ValueError("--expected-frames must be greater than zero")
    if args.columns < 0:
        raise ValueError("--columns cannot be negative")

    if not args.frames_dir.is_dir():
        raise ValueError(f"Frame directory does not exist: {args.frames_dir}")
    paths = [args.frames_dir / f"frame_{index:03d}.png" for index in range(args.expected_frames)]
    expected_names = {path.name for path in paths}
    actual_names = png_names(args.frames_dir)
    missing = sorted(expected_names - actual_names)
    unexpected = sorted(actual_names - expected_names)
    if missing or unexpected:
        raise ValueError(f"Frame sequence mismatch: missing={missing}; unexpected={unexpected}")
    output_frames = args.output_dir / "frames"
    stale_frames = sorted(png_names(output_frames) - expected_names)
    if stale_frames:
        raise ValueError(f"Output contains unrelated frames: {stale_frames}; use a new output directory")

    frames, warnings = load_frames(paths, args.allow_opaque)
    width, height = frames[0].size
    count = len(frames)
    columns = args.columns or math.ceil(math.sqrt(count))
    rows = math.ceil(count / columns)
    duration_ms = max(1, round(1000 / args.fps))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    output_frames.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (columns * width, rows * height), (0, 0, 0, 0))
    frame_info: list[dict[str, object]] = []

    for index, (path, frame) in enumerate(zip(paths, frames)):
        output_frame = output_frames / path.name
        if path.resolve() != output_frame.resolve():
            shutil.copy2(path, output_frame)
        x = (index % columns) * width
        y = (index // columns) * height
        sheet.alpha_composite(frame, (x, y))
        bbox = alpha_bbox(frame, args.alpha_threshold)
        edge_ratio = edge_alpha_ratio(frame, args.alpha_threshold)
        if bbox is None:
            warnings.append(f"{path.name}: fully transparent")
        if edge_ratio > 0:
            warnings.append(f"{path.name}: {edge_ratio:.1%} of border pixels are occupied; inspect clipping")
        frame_info.append(
            {
                "index": index,
                "file": f"frames/{path.name}",
                "bbox": list(bbox) if bbox else None,
                "edge_alpha_ratio": round(edge_ratio, 6),
                "sheet_rect": [x, y, width, height],
            }
        )

    sheet_path = args.output_dir / "spritesheet.png"
    apng_path = args.output_dir / "preview.apng"
    gif_path = args.output_dir / "preview.gif"
    manifest_path = args.output_dir / "manifest.json"

    sheet.save(sheet_path)
    frames[0].save(
        apng_path,
        format="PNG",
        save_all=True,
        append_images=frames[1:],
        duration=duration_ms,
        loop=0 if args.loop else 1,
        disposal=0,
        blend=0,
    )
    save_gif(frames, gif_path, duration_ms, args.loop)

    manifest = {
        "frame_count": count,
        "playback": "loop" if args.loop else "one-shot",
        "fps": args.fps,
        "duration_ms_per_frame": duration_ms,
        "canvas": {"width": width, "height": height},
        "sprite_sheet": {"file": sheet_path.name, "columns": columns, "rows": rows},
        "previews": {"apng": apng_path.name, "gif": gif_path.name},
        "alpha_threshold": args.alpha_threshold,
        "frames": frame_info,
        "warnings": warnings,
    }
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"Exported {count} frames at {args.fps:g} fps to {args.output_dir}")
    print(f"Canvas: {width}x{height}; sheet: {columns}x{rows}")
    if warnings:
        print(f"Warnings: {len(warnings)} (see manifest.json)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
