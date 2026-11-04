from __future__ import annotations

import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass

from .cli_arguments import parse_platform_arguments

from .configure_project_template import main as configure_project_template
from .editor_versions import read_editor_versions
from .file_replace import remove_tree
from .pack_error import PackError
from .package_process import cancellation_signals, run
from .packaging_constants import (
    CPP_TEMPLATE_NAMES,
    EDITOR_CACHE_DIRECTORY,
    NATIVE_ENUM_DIRECTORIES,
    NATIVE_LUA_FILES,
    STANDALONE_TEMPLATE_NAMES,
)
from .preview import copy_preview, load_preview
from .template_contents import (
    RUNTIME_LICENSE_DIRECTORIES,
    copy_cpp_template,
    copy_runtime_legal_files,
    copy_standalone_files,
    validate_templates,
)
from .tool_paths import resolve_toolchain_directory
from .versions import read_versions


@dataclass(frozen=True)
class TemplateBuild:
    repository: pathlib.Path
    source: pathlib.Path
    output: pathlib.Path
    configuration: str
    version: str
    ffmpeg_version: str
    native_cache: pathlib.Path | None
    platform: str


@dataclass(frozen=True)
class TemplatePair:
    source: pathlib.Path
    standalone: pathlib.Path
    ffmpeg: bool


def _is_link(path: pathlib.Path) -> bool:
    return path.is_symlink() or bool(getattr(path, "is_junction", lambda: False)())


def _check_directory_ancestors(path: pathlib.Path) -> None:
    for current in (path, *path.parents):
        if _is_link(current) or current.exists() and not current.is_dir():
            raise ValueError(f"Unsafe cache/template path: {current}")


def _overlaps(first: pathlib.Path, second: pathlib.Path) -> bool:
    return first.is_relative_to(second) or second.is_relative_to(first)


def _cache_entry(build: TemplateBuild, ffmpeg: bool) -> pathlib.Path | None:
    if build.native_cache is None:
        return None
    return build.native_cache / ("ffmpeg" if ffmpeg else "plain") / build.configuration


def _validate_cache_paths(build: TemplateBuild) -> None:
    cache = build.native_cache
    if cache is None:
        return
    for protected in (build.output, build.source):
        if _overlaps(cache.resolve(), protected.resolve()):
            raise ValueError(f"Native cache overlaps templates or Game: {cache}")
    if build.platform == "win32":
        for path in (cache, build.output, build.source):
            _check_directory_ancestors(path)
    for variant in ("plain", "ffmpeg"):
        path = cache / variant / build.configuration
        if build.platform == "win32":
            _check_directory_ancestors(path)
        elif _is_link(path) or _is_link(path.parent):
            raise ValueError(f"Native cache entries must not be symbolic links: {path}")


def _require_file(path: pathlib.Path, *, nonempty: bool = False) -> None:
    if not path.is_file() or nonempty and path.stat().st_size == 0:
        raise ValueError(f"Required template input is missing or empty: {path}")


def _validate_native_cache(build: TemplateBuild, entry: pathlib.Path) -> None:
    runtime = entry / "bin" / build.configuration
    if build.platform == "win32":
        for name in ("Main.exe", "Engine.dll", "GlobalCore.dll", "GlobalFunctions.dll", "LuaSF.dll", "LuaGlue.dll", "lua.dll"):
            _require_file(runtime / name, nonempty=True)
        _require_file(entry / "build/launcher" / build.configuration / "Main.exe", nonempty=True)
    else:
        _require_file(runtime / "Main", nonempty=True)
        if not any(
            (path.is_file() or path.is_symlink())
            and (path.suffix in (".so", ".dylib") or ".so." in path.name)
            for path in runtime.iterdir()
        ):
            raise ValueError(f"Native cache contains no runtime libraries: {entry}")
    for relative in NATIVE_LUA_FILES:
        _require_file(entry / "Scripts" / relative, nonempty=True)
    for relative in NATIVE_ENUM_DIRECTORIES:
        if not (entry / "Scripts" / relative).is_dir():
            raise ValueError(f"Native enum directory was not found: {entry / 'Scripts' / relative}")
    load_preview(entry)


def _copy_native_outputs(build: TemplateBuild, source: pathlib.Path, destination: pathlib.Path) -> None:
    directories = [pathlib.Path("bin") / build.configuration]
    if build.platform == "win32":
        directories.append(pathlib.Path("build/launcher") / build.configuration)
    for relative in directories:
        shutil.copytree(source / relative, destination / relative, dirs_exist_ok=True, symlinks=build.platform != "win32")
    for relative in NATIVE_LUA_FILES:
        target = destination / "Scripts" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / "Scripts" / relative, target)
    for relative in NATIVE_ENUM_DIRECTORIES:
        target = destination / "Scripts" / relative
        if target.exists():
            remove_tree(target)
        shutil.copytree(source / "Scripts" / relative, target, symlinks=build.platform != "win32")
    copy_preview(source, destination, runtime_directory=f"bin/{build.configuration}")


def _configure(build: TemplateBuild, project: pathlib.Path, *, cpp: bool, ffmpeg: bool) -> None:
    configure_project_template([
        str(project / "Main.proj"), str(cpp).lower(), str(ffmpeg).lower(),
        "--versions-config", str(build.repository / "versions.conf"), "--version", build.version,
    ])


def _prepare_pair(build: TemplateBuild, pair: TemplatePair) -> None:
    for target in (pair.source, pair.standalone):
        if _is_link(target):
            raise ValueError(f"Template output must not be a link: {target}")
        if target.exists():
            remove_tree(target)
        target.mkdir(parents=True)
    copy_cpp_template(
        build.source, pair.source, platform=build.platform,
        include_ffmpeg=pair.ffmpeg, ffmpeg_version=build.ffmpeg_version,
    )
    if build.platform != "win32":
        generator = pair.source / "generate_clion.sh"
        generator.chmod(generator.stat().st_mode | 0o111)
    _configure(build, pair.source, cpp=True, ffmpeg=pair.ffmpeg)
    copy_runtime_legal_files(
        build.repository, build.source, pair.source, platform=build.platform, include_ffmpeg=pair.ffmpeg,
    )


def _build_pair(build: TemplateBuild, pair: TemplatePair, dependency_cache: pathlib.Path | None = None) -> None:
    cache = _cache_entry(build, pair.ffmpeg)
    current_build = cache is not None and cache.exists()
    if current_build:
        _copy_native_outputs(build, cache, pair.source)
        print(f"Reusing native cache: {cache}", flush=True)
    environment = os.environ.copy()
    environment["LUDORK_SERVER_ENABLED"] = "0"
    for name in ("LUDORK_SERVER_CONFIG_FILE", "LUDORK_SERVER_URL", "LUDORK_SERVER_KEY"):
        environment.pop(name, None)
    if dependency_cache is not None:
        environment["LUDORK_DEPENDENCY_CACHE"] = str(dependency_cache)
    arguments = [str(pair.source), str(pair.standalone), build.configuration]
    if current_build:
        arguments.insert(0, "--use-current-build")
    if build.platform == "win32":
        environment.update({
            "LUDORK_TEMPLATE_BUILD_SCRIPT": str(build.repository / "tools/build_standalone.bat"),
            "LUDORK_TEMPLATE_BUILD_SOURCE": str(pair.source),
            "LUDORK_TEMPLATE_BUILD_OUTPUT": str(pair.standalone),
        })
        command = (
            subprocess.list2cmdline([environment.get("COMSPEC", "cmd.exe")])
            + ' /d /v:off /s /c ""%LUDORK_TEMPLATE_BUILD_SCRIPT%" '
            + ("--use-current-build " if current_build else "")
            + '"%LUDORK_TEMPLATE_BUILD_SOURCE%" "%LUDORK_TEMPLATE_BUILD_OUTPUT%" '
            + build.configuration + '"'
        )
    else:
        command = ["sh", str(build.repository / "tools/build_standalone.sh"), *arguments]
    run(command, cwd=build.repository, env=environment)
    copy_standalone_files(pair.source, pair.standalone, build.source)
    _configure(build, pair.standalone, cpp=False, ffmpeg=pair.ffmpeg)


def _finalize_pair(build: TemplateBuild, pair: TemplatePair) -> None:
    cache = _cache_entry(build, pair.ffmpeg)
    if cache is not None and not cache.exists():
        _copy_native_outputs(build, pair.source, cache)
        _validate_native_cache(build, cache)
        print(f"Saved native cache: {cache}", flush=True)
    for relative in ("build", "bin", "Intermediate", EDITOR_CACHE_DIRECTORY, "Cache", "Binaries"):
        path = pair.source / relative
        if path.exists():
            remove_tree(path)
    validate_templates(
        build.output, platform=build.platform, ffmpeg_version=build.ffmpeg_version,
        names=(pair.source.name, pair.standalone.name),
    )
    label = " FFmpeg" if pair.ffmpeg else ""
    executable = "Main.exe" if build.platform == "win32" else "Main"
    print(f"C++{label} source template is ready: {pair.source}", flush=True)
    print(f"Standalone{label} template is ready: {pair.standalone / executable}", flush=True)


def create_templates(build: TemplateBuild, variant: str) -> None:
    resolve_toolchain_directory(build.repository / ".tools/Toolchain")
    _require_file(build.repository / "tools" / ("build_standalone.bat" if build.platform == "win32" else "build_standalone.sh"))
    _require_file(build.source / "CMakeLists.txt")
    for name in ("LuaSF", "SFML", "LuaGlue", "Lua", "lua-cjson", "zlib", "IXWebSocket"):
        if not (build.source / "Engine/ThirdParty" / name).is_dir():
            raise ValueError(f"Game dependency was not found: Engine/ThirdParty/{name}. Run tools/init first.")
    for name in ("README.md", "README_zh_CN.md", *RUNTIME_LICENSE_DIRECTORIES):
        if not (build.repository / "Licenses" / name).exists():
            raise ValueError(f"Required runtime licence source was not found: {build.repository / 'Licenses' / name}")
    if variant != "plain":
        _require_file(build.source / "Engine/ThirdParty/ffmpeg/configure")
        _require_file(build.source / "ThirdPartySource" / f"ffmpeg-{build.ffmpeg_version}.tar.gz")
        if not (build.repository / "Licenses/FFmpeg").is_dir():
            raise ValueError(f"Required FFmpeg licence source was not found: {build.repository / 'Licenses/FFmpeg'}")
        if build.platform == "win32":
            _require_file(build.repository / ".tools/gnu-make/gnumake.exe")
    _validate_cache_paths(build)
    pairs = tuple(
        TemplatePair(build.output / CPP_TEMPLATE_NAMES[index], build.output / STANDALONE_TEMPLATE_NAMES[index], bool(index))
        for index, name in enumerate(("plain", "ffmpeg")) if variant in ("all", name)
    )
    for pair in pairs:
        for path in (pair.source, pair.standalone):
            if _overlaps(path.resolve(), build.source.resolve()):
                raise ValueError(f"Template output overlaps Game: {path}")
            if _is_link(path) or path.exists() and not path.is_dir():
                raise ValueError(f"Template output must be a normal directory: {path}")
        cache = _cache_entry(build, pair.ffmpeg)
        if cache is not None and cache.exists():
            _validate_native_cache(build, cache)
    if build.platform == "win32":
        for pair in pairs:
            _prepare_pair(build, pair)
            _build_pair(build, pair)
            _finalize_pair(build, pair)
    else:
        for pair in pairs:
            _prepare_pair(build, pair)
        for index, pair in enumerate(pairs):
            cache = pairs[0].source / "build/_deps" if index else None
            _build_pair(build, pair, cache)
        for pair in pairs:
            _finalize_pair(build, pair)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-pack create-templates")
    parser.add_argument("--repository-root", type=pathlib.Path, required=True)
    parser.add_argument("--version")
    parser.add_argument("--variant", choices=("all", "plain", "ffmpeg"), default="all",
                        type=str.lower if sys.platform == "win32" else str)
    parser.add_argument("--native-cache", type=pathlib.Path)
    parser.add_argument("configuration", choices=("Debug", "Release"), nargs="?", default="Release",
                        type=str.capitalize if sys.platform == "win32" else str)
    parser.add_argument("output", type=pathlib.Path, nargs="?")
    try:
        options = parse_platform_arguments(parser, arguments)
    except SystemExit as error:
        return 0 if error.code == 0 else 1
    try:
        repository = options.repository_root.expanduser().resolve()
        versions = read_versions(repository / "versions.conf")
        version = read_editor_versions(repository / "versions.conf", options.version).base_version
        ffmpeg = versions.get("FFMPEG_VERSION", "")
        if re.fullmatch(r"[0-9]+(?:\.[0-9]+)+", ffmpeg) is None:
            raise ValueError("FFMPEG_VERSION is not set to a release version in versions.conf")
        output = options.output.expanduser().absolute() if options.output else repository / "Templates"
        native_cache = options.native_cache.expanduser().absolute() if options.native_cache else None
        build = TemplateBuild(repository, repository / "Game", output, options.configuration,
                              version, ffmpeg, native_cache, sys.platform)
        with cancellation_signals():
            create_templates(build, options.variant)
        return 0
    except KeyboardInterrupt:
        print("Template creation cancelled.", file=sys.stderr)
        return 130
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return error.exit_code if isinstance(error, PackError) else 1
