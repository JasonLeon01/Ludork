#!/usr/bin/env python3
import pathlib
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile


def copy_runtime(runtime_dir: pathlib.Path, macos_dir: pathlib.Path) -> None:
    for source in runtime_dir.iterdir():
        if not source.is_file() and not source.is_symlink():
            continue
        if source.name == "Main" or source.suffix in {".dylib", ".so"} or ".so." in source.name:
            shutil.copy2(source.resolve(), macos_dir / source.name)
    executable = macos_dir / "Main"
    if not executable.exists():
        raise RuntimeError(f"Runtime is missing Main: {runtime_dir}")
    executable.chmod(executable.stat().st_mode | 0o111)


def copy_resources(project_dir: pathlib.Path, resources_dir: pathlib.Path) -> None:
    for name in ("Assets", "Data", "Scripts"):
        source = project_dir / name
        if not source.is_dir():
            raise RuntimeError(f"Project is missing {name}: {source}")
        shutil.copytree(
            source,
            resources_dir / name,
            ignore=shutil.ignore_patterns(".DS_Store", "*.anim.json"),
        )
    for name in ("Licenses", "ThirdPartySource"):
        source = project_dir / name
        if source.is_dir():
            shutil.copytree(
                source,
                resources_dir / name,
                ignore=shutil.ignore_patterns(".DS_Store"),
            )
    for name in (
        "LICENSE.md",
        "THIRD_PARTY_NOTICES.md",
        "THIRD_PARTY_NOTICES_zh_CN.md",
    ):
        source = project_dir / name
        if source.is_file():
            shutil.copy2(source, resources_dir / name)


def require_exact_entry(
    parent: pathlib.Path, name: str, entry_type: str
) -> pathlib.Path:
    if not parent.is_dir():
        raise RuntimeError(f"Bundle directory is missing: {parent}")
    entries = {entry.name: entry for entry in parent.iterdir()}
    entry = entries.get(name)
    if entry is None:
        raise RuntimeError(f"Bundle is missing exact-case {name}: {parent / name}")
    if entry_type == "directory" and not entry.is_dir():
        raise RuntimeError(f"Bundle entry is not a directory: {entry}")
    if entry_type == "file" and not entry.is_file():
        raise RuntimeError(f"Bundle entry is not a file: {entry}")
    return entry


def validate_resources(resources_dir: pathlib.Path) -> None:
    require_exact_entry(resources_dir, "Assets", "directory")
    require_exact_entry(resources_dir, "Data", "directory")
    scripts_dir = require_exact_entry(resources_dir, "Scripts", "directory")
    require_exact_entry(scripts_dir, "Entry.lua", "file")


def create_icon(project_dir: pathlib.Path, resources_dir: pathlib.Path) -> None:
    icon = project_dir / "Assets" / "System" / "icon.icns"
    if icon.is_file():
        shutil.copy2(icon, resources_dir / "AppIcon.icns")
        return
    png = project_dir / "Assets" / "System" / "icon.png"
    if not png.is_file():
        raise RuntimeError(f"Project icon is missing: {icon} or {png}")
    sips = shutil.which("sips")
    if sips is None:
        raise RuntimeError(f"Cannot generate {icon.name}: sips was not found")
    iconutil = shutil.which("iconutil")
    if iconutil is None:
        raise RuntimeError(f"Cannot generate {icon.name}: iconutil was not found")
    app_icon = resources_dir / "AppIcon.icns"
    with tempfile.TemporaryDirectory(prefix="ludork-icon-") as temporary:
        iconset = pathlib.Path(temporary) / "AppIcon.iconset"
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                pixels = size * scale
                suffix = "@2x" if scale == 2 else ""
                resized_icon = iconset / f"icon_{size}x{size}{suffix}.png"
                result = subprocess.run(
                    [sips, "-z", str(pixels), str(pixels), str(png), "--out", str(resized_icon)],
                    check=False,
                    stdout=subprocess.DEVNULL,
                )
                if result.returncode != 0:
                    raise RuntimeError(
                        f"sips failed to generate {resized_icon}: exit code {result.returncode}"
                    )
        result = subprocess.run(
            [iconutil, "-c", "icns", str(iconset), "-o", str(app_icon)],
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(
                f"iconutil failed to generate {app_icon}: exit code {result.returncode}"
            )
    if not app_icon.is_file():
        raise RuntimeError(f"iconutil did not generate {app_icon}")


def bundle_identifier(project_name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", project_name.lower()).strip("-")
    return f"com.ludork.game.{slug or 'main'}"


def main(arguments: list[str] | None = None) -> int:
    command_arguments = sys.argv[1:] if arguments is None else arguments
    if len(command_arguments) != 3:
        print(
            "Usage: ScriptTools macos-bundle <project-folder> <runtime-folder> <app-path>",
            file=sys.stderr,
        )
        return 1
    project_dir = pathlib.Path(command_arguments[0]).resolve()
    runtime_dir = pathlib.Path(command_arguments[1]).resolve()
    app_path = pathlib.Path(command_arguments[2]).resolve()
    if app_path.exists():
        shutil.rmtree(app_path)
    macos_dir = app_path / "Contents" / "MacOS"
    resources_dir = app_path / "Contents" / "Resources"
    macos_dir.mkdir(parents=True)
    resources_dir.mkdir(parents=True)
    copy_runtime(runtime_dir, macos_dir)
    copy_resources(project_dir, resources_dir)
    validate_resources(resources_dir)
    create_icon(project_dir, resources_dir)
    plist = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": project_dir.name,
        "CFBundleExecutable": "Main",
        "CFBundleIdentifier": bundle_identifier(project_dir.name),
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleIconFile": "AppIcon",
        "CFBundleName": "Main",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "1.0.0",
        "CFBundleVersion": "1",
        "LSMinimumSystemVersion": "13.3",
        "NSHighResolutionCapable": True,
    }
    with (app_path / "Contents" / "Info.plist").open("wb") as stream:
        plistlib.dump(plist, stream, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
