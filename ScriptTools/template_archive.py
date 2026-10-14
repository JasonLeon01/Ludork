from __future__ import annotations

import argparse
import os
import pathlib
import stat
import sys
import tarfile
import zipfile

from .packaging_constants import CPP_TEMPLATE_NAMES, TEMPLATE_NAMES


def _entries(root: pathlib.Path, platform: str) -> list[pathlib.Path]:
    entries = sorted(root.rglob("*"))
    for path in entries:
        if getattr(path, "is_junction", lambda: False)():
            raise ValueError(f"Template contains a directory junction: {path}")
        mode = path.lstat().st_mode
        if stat.S_ISLNK(mode):
            target = pathlib.PurePosixPath(os.readlink(path))
            if platform != "macos" or target.is_absolute() or "\\" in str(target):
                raise ValueError(f"Unsupported template link: {path}")
            if not path.resolve(strict=True).is_relative_to(root) or not path.is_file():
                raise ValueError(f"Template link must target a file inside the template: {path}")
        elif not stat.S_ISDIR(mode) and not stat.S_ISREG(mode):
            raise ValueError(f"Unsupported template entry: {path}")
        if path.relative_to(root).as_posix() in ("BuildInfo.json", "Data/BuildInfo.json"):
            raise ValueError(f"Generated build information must not be in a template: {path}")
        if path.relative_to(root).is_relative_to("Assets/Shaders/MaterialGenerated"):
            raise ValueError(f"Generated materials must not be in a template: {path}")
    return entries


def archive_template(root: pathlib.Path, destination: pathlib.Path, platform: str) -> None:
    if root.is_symlink() or not root.is_dir() or not (root / "Main.proj").is_file():
        raise ValueError(f"Project template was not found: {root}")
    root = root.resolve()
    if destination.resolve().is_relative_to(root):
        raise ValueError("Template archives must be outside their input template.")
    entries = _entries(root, platform)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if platform == "windows":
        with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as package:
            for path in entries:
                package.write(path, path.relative_to(root).as_posix())
        with zipfile.ZipFile(destination) as package:
            if package.testzip() is not None:
                raise ValueError(f"Template ZIP failed validation: {destination}")
    else:
        with tarfile.open(destination, "w:gz", dereference=False) as package:
            for path in entries:
                entry = package.gettarinfo(str(path), arcname=path.relative_to(root).as_posix())
                if entry.isfile() or entry.islnk():
                    entry.type = tarfile.REGTYPE
                    entry.linkname = ""
                    entry.size = path.stat().st_size
                    with path.open("rb") as stream:
                        package.addfile(entry, stream)
                else:
                    package.addfile(entry)
        with tarfile.open(destination, "r:gz") as package:
            for entry in package:
                if entry.isfile():
                    with package.extractfile(entry) as stream:
                        while stream.read(1024 * 1024):
                            pass
    print(destination, flush=True)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools template-archive")
    parser.add_argument("templates", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--platform", choices=("windows", "macos"), required=True)
    parsed = parser.parse_args(arguments)
    try:
        for name in TEMPLATE_NAMES:
            kind = "source" if name in CPP_TEMPLATE_NAMES else "standalone"
            variant = "-ffmpeg" if name.endswith("-ffmpeg") else ""
            suffix = ".zip" if parsed.platform == "windows" else ".tar.gz"
            archive_template(
                parsed.templates / name,
                parsed.output / f"game-{kind}{variant}{suffix}",
                parsed.platform,
            )
        return 0
    except (OSError, ValueError, tarfile.TarError, zipfile.BadZipFile) as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
