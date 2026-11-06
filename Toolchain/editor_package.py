from __future__ import annotations

import argparse
import pathlib
import platform
import shutil
import sys

from . import editor_macos_metadata, editor_official_plugins, legal_resources
from .build_tools import find_cmake
from .cli_arguments import parse_platform_arguments
from .editor_launcher_build import build as build_editor_launcher
from .editor_package_contents import (
    BUILD_TOOL_SOURCES, clean_package, copy_directory, remove_path, require_directory,
    require_file, stage_build_tools, validate_layout, validate_package,
)
from .editor_package_macos import EditorDiskImage, SIGNING_ENVIRONMENT
from .file_replace import replace_path
from .pack_error import PackError
from .package_process import run
from .packaging_metadata import read_release_version
from .runtime_bundle import validate_bundle
from .tool_paths import resolve_toolchain_directory
from .tool_process import cancellation_signals
from .versions import read_versions


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ludork-editor package")
    parser.add_argument("--repository-root", type=pathlib.Path, required=True)
    parser.add_argument("--version")
    channel = parser.add_mutually_exclusive_group()
    channel.add_argument("--dev", dest="channel", action="store_const", const="dev")
    channel.add_argument("--release", dest="channel", action="store_const", const="release")
    parser.set_defaults(channel="dev")
    templates = parser.add_mutually_exclusive_group()
    templates.add_argument("--templates", type=pathlib.Path)
    templates.add_argument("--without-templates", action="store_true")
    parser.add_argument("--use-current-editor-build", action="store_true")
    parser.add_argument("--launcher", type=pathlib.Path)
    parser.add_argument("--with-build-tools", action="store_true")
    for name in SIGNING_ENVIRONMENT:
        if name != "entitlements":
            parser.add_argument("--" + name.replace("_", "-"))
    parser.add_argument("--notarize", action="store_true")
    return parser


def _host_platform(arguments: argparse.Namespace) -> str:
    if sys.platform == "win32" and platform.machine().lower() in ("amd64", "x86_64"):
        if arguments.with_build_tools or arguments.notarize or any(
            getattr(arguments, name, None) for name in SIGNING_ENVIRONMENT
        ):
            raise PackError("macOS packaging options are not supported on Windows.")
        return "windows"
    if sys.platform == "darwin" and platform.machine() == "arm64":
        if arguments.launcher is not None:
            raise PackError("--launcher is only supported for Windows editor packages.")
        return "macos"
    raise PackError("Editor packaging requires Windows x64 or Apple Silicon macOS.")


def _preflight(repository: pathlib.Path, host: str, arguments: argparse.Namespace,
               versions: dict[str, str], templates: bool) -> pathlib.Path:
    require_file(repository / "Ludork.csproj")
    require_file(repository / "versions.conf")
    if shutil.which("dotnet") is None:
        raise PackError("The .NET SDK was not found.")
    cmake = find_cmake()
    for key in ("EDITOR_VERSION", "FFMPEG_VERSION") + (("GNU_MAKE_VERSION",) if host == "windows" else ()):
        if not versions.get(key):
            raise PackError(f"{key} is not set in versions.conf.")
    for name in ("Locale/locale.json", "LICENSE.md", "README.md", "README_zh_CN.md"):
        require_file(repository / name)
    require_directory(repository / "Licenses")
    for source in BUILD_TOOL_SOURCES[host].values():
        require_file(repository / source)
    if host == "windows":
        require_file(repository / "tools/editor_launcher/CMakeLists.txt")
        require_file(repository / ".tools/gnu-make/gnumake.exe")
        require_file(repository / f".tools/sources/make-{versions['GNU_MAKE_VERSION']}.tar.gz")
    else:
        require_file(repository / "Editor/Assets/icon.ico")
        require_file(repository / "Editor/Assets/project-icon.png")
    if arguments.launcher is not None:
        require_file(arguments.launcher)
    if templates:
        require_file(repository / "Game/CMakeLists.txt")
        for name in ("LuaSF", "SFML", "LuaGlue", "Lua", "lua-cjson", "zlib", "IXWebSocket"):
            require_directory(repository / "Game/Engine/ThirdParty" / name)
        require_file(repository / "Game/Engine/ThirdParty/ffmpeg/configure")
        require_file(repository / f"Game/ThirdPartySource/ffmpeg-{versions['FFMPEG_VERSION']}.tar.gz")
    return cmake


def publish_package(stage: pathlib.Path, destination: pathlib.Path, backup: pathlib.Path) -> None:
    if backup.exists() or backup.is_symlink():
        raise PackError(f"A preserved editor package backup requires manual recovery: {backup}")
    backed_up = False
    if destination.exists() or destination.is_symlink():
        replace_path(destination, backup)
        backed_up = True
    try:
        replace_path(stage, destination)
    except BaseException as original:
        if backed_up:
            try:
                replace_path(backup, destination)
            except (OSError, RuntimeError) as error:
                raise PackError(f"Editor package publication failed: {original}\n"
                                f"Failed to restore the previous package; its backup is preserved at {backup}: {error}") from original
        raise
    if backed_up:
        try:
            remove_path(backup)
        except (OSError, RuntimeError) as error:
            raise PackError(f"The editor package was published, but its previous backup could not be removed: {backup}: {error}") from error


def _publish_editor(repository: pathlib.Path, binaries: pathlib.Path, host: str,
                    arguments: argparse.Namespace, product_version: str, build_info: pathlib.Path) -> None:
    identifier = "win-x64" if host == "windows" else "osx-arm64"
    properties = ["-p:Configuration=Release", "-p:SelfContained=true", "-p:PublishSingleFile=false",
                  "-p:PublishTrimmed=false", "-p:PublishAot=false", "-p:DebugSymbols=false", "-p:DebugType=None"]
    if host == "windows":
        properties.append("-p:RestoreLockedMode=true")
    if arguments.use_current_editor_build:
        run(["dotnet", "restore", repository / "Ludork.csproj", "-r", identifier, *properties], cwd=repository)
    command: list[str | pathlib.Path] = ["dotnet", "publish", repository / "Ludork.csproj"]
    if arguments.use_current_editor_build:
        command.append("--no-build")
    command.extend(("-c", "Release", "-r", identifier, "--self-contained", "true", "-o", binaries,
                    *properties, f"-p:Version={product_version}", f"-p:LudorkBuildInfo={build_info}"))
    run(command, cwd=repository)
    executable = binaries / ("Ludork.exe" if host == "windows" else "Ludork")
    run([executable, "--check-build-info", build_info], cwd=repository)
    if host == "windows":
        from .prune_editor_windows_publish import FOREIGN_ASSEMBLIES, FOREIGN_PACKAGES, prunePublish
    else:
        from .prune_editor_macos_publish import FOREIGN_ASSEMBLIES, FOREIGN_PACKAGES, prunePublish
    prunePublish(binaries, FOREIGN_PACKAGES, FOREIGN_ASSEMBLIES)


def _relocate_content(repository: pathlib.Path, package: pathlib.Path, binaries: pathlib.Path,
                      content: pathlib.Path, host: str, channel: str) -> None:
    if host == "windows":
        for name in ("Locale", "Licenses", "LICENSE.md", "README.md", "README_zh_CN.md"):
            replace_path(binaries / name, content / name)
        remove_path(binaries / "Page")
        legal_resources.copy_resources(repository, content, "editor")
        validate_layout(package, host, channel)
        run([package / "Ludork.exe", "--compile-locale"], cwd=package)
        remove_path(content / "Locale/locale.json")
    else:
        run([binaries / "Ludork", "--compile-locale"], cwd=binaries)
        remove_path(binaries / "Locale/locale.json")
        copy_directory(binaries / "Locale", content / "Locale")
        for name in ("Locale", "docs", "Page", "Licenses", "LICENSE.md", "README.md", "README_zh_CN.md",
                     "THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES_zh_CN.md"):
            remove_path(binaries / name)
        for path in binaries.glob("About_*.md"):
            remove_path(path)
        for name in ("docs", "Page"):
            remove_path(content / name)
        copy_directory(repository / "Licenses", content / "Licenses")
        legal_resources.copy_resources(repository, content, "editor")


def package(arguments: argparse.Namespace) -> pathlib.Path:
    host = _host_platform(arguments)
    repository = arguments.repository_root.expanduser().resolve()
    versions = read_versions(repository / "versions.conf")
    if arguments.channel == "release" and arguments.templates is not None:
        raise PackError("Release editors download templates and cannot bundle --templates.")
    include_templates = arguments.channel == "dev" and not arguments.without_templates
    work = repository / "obj/editor-package"
    stage = work / "dist"
    destination = repository / "dist"
    backup = work / "previous-dist"
    if work.is_symlink() or work.exists() and not work.is_dir():
        raise PackError(f"The editor package work path is not a normal directory: {work}")
    if backup.exists() or backup.is_symlink():
        raise PackError(f"A preserved editor package backup requires manual recovery: {backup}")
    if arguments.templates is not None:
        arguments.templates = arguments.templates.expanduser().resolve()
        require_directory(arguments.templates)
        if arguments.templates.is_relative_to(work):
            raise PackError(f"The prepared template folder must remain outside {work}.")
    if arguments.launcher is not None:
        arguments.launcher = arguments.launcher.expanduser().resolve()
        if arguments.launcher.is_relative_to(work):
            raise PackError(f"The prepared launcher must remain outside {work}.")
    toolchain = resolve_toolchain_directory(repository / ".tools/Toolchain")
    validate_bundle(toolchain)
    cmake = _preflight(repository, host, arguments, versions, include_templates)
    release = read_release_version(arguments.version or versions["EDITOR_VERSION"], arguments.channel == "dev")
    build_info = repository / "obj/editor-package-metadata/BuildInfo.json"
    disk_image = EditorDiskImage(repository, work, stage, toolchain, arguments) if host == "macos" else None
    if disk_image is not None:
        disk_image.preflight()
    work_owned = False
    try:
        remove_path(work)
        work_owned = True
        stage.mkdir(parents=True)
        package_root = disk_image.app if disk_image is not None else stage
        binaries = disk_image.binaries if disk_image is not None else stage / "Binaries"
        content = disk_image.resources if disk_image is not None else stage
        binaries.mkdir(parents=True, exist_ok=True)
        content.mkdir(parents=True, exist_ok=True)
        release.write_build_info(build_info.parent)
        if release.dev:
            shutil.copy2(build_info, content / "BuildInfo.json")
        print(f"Packaging Ludork {release.full_version} for {host}...", flush=True)
        if host == "windows":
            launcher = arguments.launcher
            if launcher is None:
                launcher = build_editor_launcher(repository, work / "launcher", release.version, cmake=cmake)
            shutil.copy2(launcher, stage / "Ludork.exe")
        _publish_editor(repository, binaries, host, arguments, release.version, build_info)
        _relocate_content(repository, package_root, binaries, content, host, arguments.channel)
        if include_templates:
            from . import create_templates
            from .template_contents import copy_prebuilt_templates
            if arguments.templates is not None:
                copy_prebuilt_templates(arguments.templates, content / "Templates", platform=sys.platform)
            else:
                result = create_templates.main(["--repository-root", str(repository), "--version", release.version,
                                                "Release", str(content / "Templates")])
                if result:
                    raise PackError("Editor project template generation failed.", result)
        if host == "windows":
            plugins = content / "BuildTools/OfficialPlugins" if arguments.channel == "release" else content
            editor_official_plugins.prepare(repository / "Plugins", plugins)
        stage_build_tools(repository, content / "BuildTools", host, versions)
        if disk_image is not None:
            editor_macos_metadata.generate(build_info, package_root / "Contents/Info.plist")
            disk_image.create_icons()
        clean_package(package_root, host, templates=include_templates)

        def validate(root: pathlib.Path) -> None:
            validate_package(root, platform=host, channel=arguments.channel, templates=include_templates,
                             build_info=build_info, gnu_make_version=versions.get("GNU_MAKE_VERSION", ""),
                             ffmpeg_version=versions["FFMPEG_VERSION"])

        if disk_image is not None:
            from .editor_build_metadata import validate_payload
            validate_payload(package_root, build_info, content)
            output = disk_image.create(release.full_version, templates=include_templates, validate_app=validate)
            relative_output = output.relative_to(stage)
        else:
            validate(package_root)
            relative_output = pathlib.Path(".")
        publish_package(stage, destination, backup)
        result = destination / relative_output
    finally:
        cleanup_ready = disk_image.cleanup() if disk_image is not None else True
        if work_owned and cleanup_ready and not (backup.exists() or backup.is_symlink()):
            remove_path(work)
    print(f"Editor package complete: {result} ({release.full_version})", flush=True)
    return result


def main(arguments: list[str] | None = None) -> int:
    try:
        try:
            parsed = parse_platform_arguments(create_parser(), arguments)
        except SystemExit as error:
            return 0 if error.code == 0 else 1
        with cancellation_signals():
            package(parsed)
        return 0
    except KeyboardInterrupt:
        print("Editor packaging cancelled.", file=sys.stderr)
        return 130
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Editor packaging failed: {error}", file=sys.stderr)
        return error.exit_code if isinstance(error, PackError) else 1
