#!/usr/bin/env python3
import argparse
import pathlib
import plistlib
import re
import shutil
import subprocess
import tempfile

from .resource_constants import ANIMATION_CACHE_SUFFIX
from .packaging_constants import (
    RUNTIME_LEGAL_FILES,
)
from .packaging_names import prepare_directory
from .packaging_metadata import add_package_arguments, load_package_metadata, read_package_metadata
from .resource_constants import RESOURCE_GROUPS
from .ui_preview import is_preview_development_file


def is_runtime_library(path: pathlib.Path) -> bool:
    return path.suffix in {".dylib", ".so"} or ".so." in path.name


def runtime_symlink_target(
    source: pathlib.Path, binaries_dir: pathlib.Path
) -> pathlib.Path:
    target = source.readlink()
    if target.is_absolute():
        raise RuntimeError(f"Absolute Binaries symlink is unsupported: {source}")
    if target.parent != pathlib.Path("."):
        raise RuntimeError(f"Binaries symlink leaves its directory: {source}")
    target_source = binaries_dir / target
    if target_source.is_symlink():
        raise RuntimeError(f"Binaries symlink chain is unsupported: {source}")
    if target_source.is_dir():
        raise RuntimeError(f"Binaries symlink targets a directory: {source}")
    if not target_source.is_file():
        raise RuntimeError(f"Binaries symlink target is missing: {source}")
    if not is_runtime_library(target_source):
        raise RuntimeError(f"Binaries symlink target is not a library: {source}")
    return target


def copy_runtime(
    runtime_dir: pathlib.Path,
    macos_dir: pathlib.Path,
    frameworks_dir: pathlib.Path,
) -> None:
    executable_source = runtime_dir / "Main"
    if not executable_source.is_file():
        raise RuntimeError(f"Runtime is missing Main: {runtime_dir}")
    unexpected = next(
        (
            path
            for path in runtime_dir.iterdir()
            if (path.is_file() or path.is_symlink()) and is_runtime_library(path)
        ),
        None,
    )
    if unexpected is not None:
        raise RuntimeError(f"Runtime library exists outside Binaries: {unexpected}")

    binaries_dir = runtime_dir / "Binaries"
    if not binaries_dir.is_dir():
        raise RuntimeError(f"Runtime is missing Binaries: {runtime_dir}")
    runtime_files: list[pathlib.Path] = []
    runtime_symlinks: list[tuple[pathlib.Path, pathlib.Path]] = []
    for source in sorted(binaries_dir.iterdir(), key=lambda path: path.name):
        if is_preview_development_file(source.name):
            continue
        if source.is_symlink():
            if not is_runtime_library(source):
                raise RuntimeError(f"Unsupported Binaries symlink: {source}")
            runtime_symlinks.append(
                (source, runtime_symlink_target(source, binaries_dir))
            )
            continue
        if not source.is_file():
            raise RuntimeError(f"Unsupported Binaries entry: {source}")
        if not is_runtime_library(source):
            raise RuntimeError(f"Unsupported Binaries file: {source}")
        runtime_files.append(source)
    if not runtime_files:
        raise RuntimeError(f"Runtime contains no libraries in Binaries: {runtime_dir}")
    for source in runtime_files:
        shutil.copy2(source, frameworks_dir / source.name)
    for source, target in runtime_symlinks:
        destination = frameworks_dir / source.name
        destination.symlink_to(target)
        if destination.readlink() != target or not destination.is_file():
            raise RuntimeError(f"Frameworks symlink is invalid: {destination}")

    executable = macos_dir / "Main"
    shutil.copy2(executable_source.resolve(), executable)
    executable.chmod(executable.stat().st_mode | 0o111)
    rewrite_executable_rpath(executable)
    strip_and_sign_runtime(
        [executable, *(frameworks_dir / source.name for source in runtime_files)]
    )


def executable_rpaths(executable: pathlib.Path) -> set[str]:
    result = subprocess.run(
        ["otool", "-l", str(executable)],
        check=True,
        capture_output=True,
        text=True,
    )
    rpaths: set[str] = set()
    lines = iter(result.stdout.splitlines())
    for line in lines:
        if line.strip() != "cmd LC_RPATH":
            continue
        next(lines, None)
        path_line = next(lines, "").strip()
        match = re.match(r"path (.+) \(offset \d+\)$", path_line)
        if match is not None:
            rpaths.add(match.group(1))
    return rpaths


def rewrite_executable_rpath(executable: pathlib.Path) -> None:
    install_name_tool = shutil.which("install_name_tool")
    if install_name_tool is None:
        raise RuntimeError("install_name_tool was not found")
    rpaths = executable_rpaths(executable)
    if "@loader_path/Binaries" not in rpaths:
        raise RuntimeError(
            f"Runtime Main is missing @loader_path/Binaries: {executable}"
        )
    command = [install_name_tool, "-delete_rpath", "@loader_path/Binaries"]
    if "@loader_path" in rpaths:
        command.extend(["-delete_rpath", "@loader_path"])
    if "@loader_path/../Frameworks" not in rpaths:
        command.extend(["-add_rpath", "@loader_path/../Frameworks"])
    command.append(str(executable))
    subprocess.run(command, check=True)
    rewritten_rpaths = executable_rpaths(executable)
    if "@loader_path/Binaries" in rewritten_rpaths:
        raise RuntimeError(f"Bundle Main retains the Binaries rpath: {executable}")
    if "@loader_path" in rewritten_rpaths:
        raise RuntimeError(f"Bundle Main retains the root runtime search path: {executable}")
    if "@loader_path/../Frameworks" not in rewritten_rpaths:
        raise RuntimeError(f"Bundle Main is missing the Frameworks rpath: {executable}")


def strip_and_sign_runtime(binaries: list[pathlib.Path]) -> None:
    strip = shutil.which("strip")
    codesign = shutil.which("codesign")
    if strip is None or codesign is None:
        raise RuntimeError("strip and codesign are required for macOS packaging")
    original_size = sum(binary.stat().st_size for binary in binaries)
    for binary in binaries:
        subprocess.run([strip, "-x", str(binary)], check=True)
        subprocess.run(
            [codesign, "--force", "--sign", "-", str(binary)], check=True
        )
        subprocess.run(
            [codesign, "--verify", "--strict", str(binary)], check=True
        )
    packaged_size = sum(binary.stat().st_size for binary in binaries)
    print(
        f"Runtime symbols stripped: {original_size:,} -> {packaged_size:,} bytes "
        f"({original_size - packaged_size:,} bytes saved)"
    )


def copy_resources(project_dir: pathlib.Path, resources_dir: pathlib.Path) -> None:
    for name in RESOURCE_GROUPS:
        source = project_dir / name
        if not source.is_dir():
            raise RuntimeError(f"Project is missing {name}: {source}")
        shutil.copytree(
            source,
            resources_dir / name,
            ignore=shutil.ignore_patterns(".DS_Store", "*" + ANIMATION_CACHE_SUFFIX),
        )
    for name in ("Licenses", "ThirdPartySource"):
        source = project_dir / name
        if source.is_dir():
            shutil.copytree(
                source,
                resources_dir / name,
                ignore=shutil.ignore_patterns(".DS_Store"),
            )
    for name in RUNTIME_LEGAL_FILES:
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


def bundle_identifier(app_name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", app_name.lower()).strip("-")
    return f"com.ludork.game.{slug or 'main'}"


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools macos-bundle")
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("runtime", type=pathlib.Path)
    parser.add_argument("app_path", type=pathlib.Path)
    parser.add_argument("--metadata", type=pathlib.Path)
    add_package_arguments(parser)
    parsed = parser.parse_args(arguments)
    if parsed.metadata is not None and (parsed.version is not None or parsed.dev is not None):
        parser.error("--metadata cannot be combined with version overrides")
    project_dir = parsed.project.resolve()
    metadata = (load_package_metadata(parsed.metadata) if parsed.metadata is not None
                else read_package_metadata(project_dir, parsed.version, parsed.dev))
    runtime_dir = parsed.runtime.resolve()
    app_path = prepare_directory(project_dir, parsed.app_path)
    macos_dir = app_path / "Contents" / "MacOS"
    frameworks_dir = app_path / "Contents" / "Frameworks"
    resources_dir = app_path / "Contents" / "Resources"
    macos_dir.mkdir(parents=True)
    frameworks_dir.mkdir(parents=True)
    resources_dir.mkdir(parents=True)
    copy_runtime(runtime_dir, macos_dir, frameworks_dir)
    copy_resources(project_dir, resources_dir)
    metadata.write_build_info(resources_dir)
    validate_resources(resources_dir)
    create_icon(project_dir, resources_dir)
    plist = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": metadata.display_name,
        "CFBundleExecutable": "Main",
        "CFBundleIdentifier": bundle_identifier(metadata.app_name),
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleIconFile": "AppIcon",
        "CFBundleName": metadata.display_name,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": metadata.version,
        "CFBundleVersion": metadata.apple_build_version,
        "LSMinimumSystemVersion": "13.3",
        "NSHighResolutionCapable": True,
    }
    with (app_path / "Contents" / "Info.plist").open("wb") as stream:
        plistlib.dump(plist, stream, sort_keys=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
