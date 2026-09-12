from __future__ import annotations

import argparse
import json
import math
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from contextlib import ExitStack
from dataclasses import dataclass

from PIL import Image

from .resource_constants import ASSET_PATH_PREFIX
from .packaging_constants import EDITOR_CACHE_DIRECTORY


@dataclass(frozen=True)
class KeyFrame:
    time: float
    x: float
    y: float
    rotation: float
    scale_x: float
    scale_y: float


@dataclass(frozen=True)
class Segment:
    kind: str
    asset: pathlib.Path | None
    start: KeyFrame
    end: KeyFrame
    flip_x: bool
    original_duration: float | None


def number(value: object, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError(f"{label} must be a finite number")
    return float(value)


def key_frame(value: object, label: str) -> KeyFrame:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be an object")
    components = []
    for key, default in (("position", [0, 0]), ("scale", [1, 1])):
        vector = value.get(key, default)
        if not isinstance(vector, list) or len(vector) != 2:
            raise ValueError(f"{label}.{key} must contain two numbers")
        components.extend(number(item, f"{label}.{key}") for item in vector)
    return KeyFrame(
        number(value.get("time", 0), f"{label}.time"),
        *components[:2],
        number(value.get("rotation", 0), f"{label}.rotation"),
        *components[2:],
    )


def asset_path(project: pathlib.Path, value: object) -> pathlib.Path | None:
    if value == "":
        return None
    if not isinstance(value, str) or not value.startswith(ASSET_PATH_PREFIX):
        raise ValueError(f"Expected a /Game/Assets/ path: {value!r}")
    relative = value[len("/Game/"):]
    parts = relative.split("/")
    if value != value.strip() or any(part in {"", ".", ".."} for part in parts) or any(c in value for c in "\\:\0"):
        raise ValueError(f"Invalid asset path: {value!r}")
    path = project
    for part in parts:
        if not path.is_dir() or part not in {child.name for child in path.iterdir()}:
            raise ValueError(f"Missing asset or incorrect path casing: {value}")
        path /= part
    if not path.resolve().is_relative_to((project / "Assets").resolve()) or not path.is_file():
        raise ValueError(f"Asset must be a file inside the project's Assets directory: {value}")
    return path


def load_animation(source: pathlib.Path, project: pathlib.Path) -> tuple[int, list[Segment]]:
    data = json.loads(source.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict) or data.get("type") != "animation":
        raise ValueError("Input must be a source animation JSON (type: animation)")
    fps = data.get("frameRate", 30)
    if isinstance(fps, bool) or not isinstance(fps, int) or fps <= 0:
        raise ValueError("frameRate must be a positive integer")
    assets = data.get("assets", [])
    timelines = data.get("timeLines", [])
    if not isinstance(assets, list) or not isinstance(timelines, list):
        raise ValueError("assets and timeLines must be arrays")
    paths: dict[int, pathlib.Path | None] = {}
    segments = []
    for track_index, timeline in enumerate(timelines):
        if not isinstance(timeline, dict) or not isinstance(timeline.get("timeSegments"), list):
            raise ValueError(f"timeLines[{track_index}].timeSegments must be an array")
        for segment_index, entry in enumerate(timeline["timeSegments"]):
            label = f"timeLines[{track_index}].timeSegments[{segment_index}]"
            if not isinstance(entry, dict) or entry.get("type", "frame") not in ("frame", "sound"):
                raise ValueError(f"{label} must be a frame or sound segment")
            index = entry.get("asset", -1)
            if isinstance(index, bool) or not isinstance(index, int) or index < -1 or index >= len(assets):
                raise ValueError(f"{label}.asset is out of range")
            if index not in paths:
                paths[index] = asset_path(project, assets[index]) if index >= 0 else None
            start = key_frame(entry.get("startFrame"), f"{label}.startFrame")
            end = key_frame(entry.get("endFrame"), f"{label}.endFrame")
            if start.time < 0 or end.time < start.time:
                raise ValueError(f"{label} requires 0 <= start time <= end time")
            flip = entry.get("flipX", False)
            if not isinstance(flip, bool):
                raise ValueError(f"{label}.flipX must be a boolean")
            original = entry.get("originalDuration")
            if original is not None:
                original = number(original, f"{label}.originalDuration")
                if original < 0:
                    raise ValueError(f"{label}.originalDuration cannot be negative")
            segments.append(Segment(entry.get("type", "frame"), paths[index], start, end, flip, original))
    if not segments:
        raise ValueError("Animation has no timeline segments to export")
    return fps, segments


def transform_at(segment: Segment, time: float) -> tuple[float, float, float, float, float] | None:
    start, end = segment.start, segment.end
    if time < start.time - 1e-9 or time > end.time + 1e-9:
        return None
    duration = end.time - start.time
    factor = 0 if duration <= 1e-4 else min(1, max(0, (time - start.time) / duration))
    x = start.x + (end.x - start.x) * factor
    y = start.y + (end.y - start.y) * factor
    rotation = start.rotation + (end.rotation - start.rotation) * factor
    scale_x = (start.scale_x + (end.scale_x - start.scale_x) * factor) * (-1 if segment.flip_x else 1)
    scale_y = start.scale_y + (end.scale_y - start.scale_y) * factor
    return x, y, math.radians(rotation), scale_x, scale_y


def canvas_size(segments: list[Segment], images: dict[pathlib.Path, Image.Image], count: int, fps: int) -> tuple[int, int]:
    max_x = max_y = 0.0
    for frame in range(count):
        for segment in segments:
            transform = transform_at(segment, frame / fps)
            if segment.asset not in images or transform is None:
                continue
            x, y, angle, sx, sy = transform
            image = images[segment.asset]
            cosine, sine = abs(math.cos(angle)), abs(math.sin(angle))
            width = image.width * abs(sx) * cosine + image.height * abs(sy) * sine
            height = image.width * abs(sx) * sine + image.height * abs(sy) * cosine
            max_x = max(max_x, abs(x) + width / 2)
            max_y = max(max_y, abs(y) + height / 2)
    return max(2, math.ceil(max_x) * 2), max(2, math.ceil(max_y) * 2)


def render_frame(segments: list[Segment], images: dict[pathlib.Path, Image.Image], time: float,
                 size: tuple[int, int], background: tuple[int, int, int]) -> Image.Image:
    canvas = Image.new("RGBA", size, (*background, 255))
    for segment in segments:
        transform = transform_at(segment, time)
        if segment.asset not in images or transform is None:
            continue
        x, y, angle, sx, sy = transform
        if sx == 0 or sy == 0:
            continue
        image = images[segment.asset]
        cosine, sine = math.cos(angle), math.sin(angle)
        cx, cy = size[0] / 2 + x, size[1] / 2 + y
        inverse = (
            cosine / sx, sine / sx, image.width / 2 - (cosine * cx + sine * cy) / sx,
            -sine / sy, cosine / sy, image.height / 2 + (sine * cx - cosine * cy) / sy,
        )
        with image.transform(size, Image.Transform.AFFINE, inverse, Image.Resampling.NEAREST) as layer:
            canvas.alpha_composite(layer)
    result = canvas.convert("RGB")
    canvas.close()
    return result


def audio_arguments(segments: list[Segment], fps: int, duration: float) -> list[str]:
    arguments = []
    filters = []
    for segment in segments:
        if segment.kind != "sound" or segment.asset is None:
            continue
        index = len(filters) + 1
        arguments.extend(["-i", str(segment.asset)])
        start_frame = max(0, math.floor(segment.start.time * fps + 1e-5))
        end_frame = max(0, math.ceil(segment.end.time * fps - 1e-5))
        trim = ""
        if segment.original_duration and segment.end.time - segment.start.time + 1e-5 < segment.original_duration:
            trim = f"atrim=duration={(end_frame - start_frame) / fps:.12f},"
        delay = round(start_frame / fps * 48000)
        filters.append(
            f"[{index}:a:0]aresample=48000,{trim}asetpts=PTS-STARTPTS,"
            f"adelay={delay}S:all=1[a{index}]"
        )
    if not filters:
        return ["-map", "0:v:0", "-an"]
    labels = "".join(f"[a{index}]" for index in range(1, len(filters) + 1))
    filters.append(
        f"{labels}amix=inputs={len(filters)}:normalize=0,apad,atrim=duration={duration:.12f}[audio]"
    )
    return arguments + ["-filter_complex", ";".join(filters), "-map", "0:v:0", "-map", "[audio]",
                        "-c:a", "aac", "-b:a", "192k"]


def export_animation(source: pathlib.Path, output: pathlib.Path, project: pathlib.Path,
                     ffmpeg: str, size: tuple[int, int] | None, background: tuple[int, int, int],
                     mute: bool, overwrite: bool) -> None:
    if output.suffix.lower() != ".mp4":
        raise ValueError("Output filename must end in .mp4")
    if output.exists() and not overwrite:
        raise ValueError(f"Output already exists (use --overwrite to replace it): {output}")
    fps, segments = load_animation(source, project)
    count = max(1, math.ceil(max(segment.end.time for segment in segments) * fps - 1e-7))
    duration = count / fps
    visuals = [segment for segment in segments if segment.kind == "frame" and segment.asset is not None]
    with ExitStack() as stack:
        images = {}
        for segment in visuals:
            if segment.asset not in images:
                with Image.open(segment.asset) as image:
                    images[segment.asset] = stack.enter_context(image.convert("RGBA"))
        size = size or canvas_size(visuals, images, count, fps)
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = pathlib.Path(stack.enter_context(tempfile.TemporaryDirectory(prefix=".animation-mp4-", dir=output.parent)))
        video = temporary / "output.mp4"
        command = [ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y",
                   "-f", "rawvideo", "-pixel_format", "rgb24", "-video_size", f"{size[0]}x{size[1]}",
                   "-framerate", str(fps), "-i", "pipe:0"]
        command += ["-map", "0:v:0", "-an"] if mute else audio_arguments(segments, fps, duration)
        command += ["-c:v", "libx264", "-crf", "18", "-preset", "medium", "-pix_fmt", "yuv420p",
                    "-t", f"{duration:.12f}", "-movflags", "+faststart", str(video)]
        print(f"{source.name}: {size[0]}x{size[1]}, {fps} fps, {count} frames, {duration:.3f}s", flush=True)
        with tempfile.TemporaryFile() as errors:
            process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=errors,
                                       creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            try:
                try:
                    for frame in range(count):
                        with render_frame(visuals, images, frame / fps, size, background) as image:
                            process.stdin.write(image.tobytes())
                    process.stdin.close()
                except BrokenPipeError:
                    pass
                result = process.wait()
                if result:
                    errors.seek(0)
                    raise ValueError(f"FFmpeg failed ({result}): {errors.read().decode('utf-8', errors='replace').strip()}")
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                try:
                    process.stdin.close()
                except BrokenPipeError:
                    pass
        if output.exists() and not overwrite:
            raise ValueError(f"Output appeared during conversion: {output}")
        video.replace(output)
    print(f"Saved: {output}", flush=True)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools animation-mp4", description="Export Ludork source animation JSON to H.264/AAC MP4.")
    parser.add_argument("project", nargs="?", type=pathlib.Path, default=pathlib.Path("Game"))
    parser.add_argument("--input", type=pathlib.Path, help="JSON file or directory; default: <project>/Data/Animations")
    parser.add_argument("--output", type=pathlib.Path, help=f"MP4 file for a single input, or output directory; default: <project>/{EDITOR_CACHE_DIRECTORY}/AnimationMp4")
    parser.add_argument("--ffmpeg", help="FFmpeg executable with libx264/AAC; default: PATH, then .tools/ffmpeg/ffmpeg[.exe]")
    parser.add_argument("--size", help="Even WIDTHxHEIGHT; default: fit all frames around the animation origin")
    parser.add_argument("--background", default="#000000", help="Opaque #RRGGBB background (default: black)")
    parser.add_argument("--mute", action="store_true", help="Export without sound")
    parser.add_argument("--overwrite", action="store_true", help="Replace existing MP4 files after successful encoding")
    parsed = parser.parse_args(arguments)
    try:
        project = parsed.project.resolve()
        if not (project / "Assets").is_dir():
            raise ValueError(f"Project Assets directory not found: {project}")
        size = None
        if parsed.size:
            match = re.fullmatch(r"([0-9]+)x([0-9]+)", parsed.size)
            if not match or any(int(value) < 2 or int(value) % 2 for value in match.groups()):
                raise ValueError("--size must be even positive dimensions, for example 512x512")
            size = tuple(int(value) for value in match.groups())
        if not re.fullmatch(r"#[0-9a-fA-F]{6}", parsed.background):
            raise ValueError("--background must be #RRGGBB")
        background = tuple(int(parsed.background[index:index + 2], 16) for index in (1, 3, 5))
        local_ffmpeg = pathlib.Path(".tools/ffmpeg") / ("ffmpeg.exe" if sys.platform == "win32" else "ffmpeg")
        ffmpeg = shutil.which(parsed.ffmpeg) if parsed.ffmpeg else shutil.which("ffmpeg") or shutil.which(str(local_ffmpeg.resolve()))
        if not ffmpeg:
            raise ValueError("FFmpeg was not found. Install an FFmpeg build with libx264/AAC and add it to PATH, or pass --ffmpeg <executable>.")
        source = (parsed.input or project / "Data" / "Animations").resolve()
        output = (parsed.output or project / EDITOR_CACHE_DIRECTORY / "AnimationMp4").resolve()
        if source.is_file():
            sources = [source]
        elif source.is_dir():
            sources = sorted(source.rglob("*.json"))
            if output.suffix.lower() == ".mp4":
                raise ValueError("Directory input requires an output directory")
        else:
            raise ValueError(f"Input does not exist: {source}")
        if not sources:
            raise ValueError(f"No animation JSON files found: {source}")
        failures = 0
        for item in sources:
            destination = output if source.is_file() and output.suffix.lower() == ".mp4" else output / (item.name if source.is_file() else item.relative_to(source))
            if destination != output:
                destination = destination.with_suffix(".mp4")
            try:
                export_animation(item, destination, project, ffmpeg, size, background, parsed.mute, parsed.overwrite)
            except (OSError, ValueError) as error:
                print(f"{item}: {error}", file=sys.stderr)
                failures += 1
        return 1 if failures else 0
    except (OSError, ValueError) as error:
        print(f"animation-mp4: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("animation-mp4: cancelled", file=sys.stderr)
        return 130
