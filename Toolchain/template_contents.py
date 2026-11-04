from __future__ import annotations

import fnmatch
import os
import pathlib
import shutil

from .file_replace import remove_file, remove_tree
from .legal_resources import copy_resources
from .ldpak import validate_ldpak_source
from .package_process import run
from .packaging_constants import (
    CPP_TEMPLATE_NAMES,
    EDITOR_CACHE_DIRECTORY,
    NATIVE_ENUM_DIRECTORIES,
    NATIVE_LUA_FILES,
    RUNTIME_LEGAL_FILES,
    TEMPLATE_NAMES,
)
from .preview import load_preview


RUNTIME_LICENSE_DIRECTORIES = (
    "Lua", "LuaSF", "SFML", "LuaGlue", "lua-cjson", "zlib", "IXWebSocket",
    "NativeDependencies",
)
RUNTIME_LICENSE_FILES = (
    "README.md", "README_zh_CN.md", "Lua/LICENSE.txt", "LuaSF/LICENSE.txt",
    "SFML/LICENSE.txt", "LuaGlue/LICENSE", "lua-cjson/LICENSE.txt", "zlib/LICENSE.txt",
    "IXWebSocket/LICENSE.txt", "IXWebSocket/THIRD-PARTY.txt",
    "NativeDependencies/FLAC-COPYING.Xiph.txt", "NativeDependencies/FreeType-FTL.txt",
    "NativeDependencies/FreeType-LICENSE.txt", "NativeDependencies/Glad-CC0-1.0.txt",
    "NativeDependencies/HarfBuzz-COPYING.txt", "NativeDependencies/libssh2-COPYING.txt",
    "NativeDependencies/MbedTLS-LICENSE.txt", "NativeDependencies/Ogg-COPYING.txt",
    "NativeDependencies/SFML-THIRD-PARTY.md", "NativeDependencies/SheenBidi-LICENSE.txt",
    "NativeDependencies/Vorbis-COPYING.txt", "NativeDependencies/Wine-DInput-LGPLv2.1.txt",
)
FFMPEG_LICENSE_FILES = (
    "COPYING.GPLv2.txt", "COPYING.GPLv3.txt", "COPYING.LGPLv2.1.txt",
    "COPYING.LGPLv3.txt", "README.md", "UPSTREAM-LICENSE.md",
)
NON_RUNTIME_LICENSE_DIRECTORIES = (
    "Avalonia", "DotNet", "DotNetPackages", "EditorPackages", "GNUMake",
    "HarmonyOSSans", "MicrosoftVisualCppRuntime", "SampleMusic", "ScriptTools",
)
RUNTIME_STATE_PATHS = (
    "Log", "Save", "Main.ini", "Main.plist", "Ludork.ini", "Ludork.plist",
    "Ludork-startup-error.log",
)
GENERATED_IDE_PATHS = (".vs", ".idea", "cmake-build-ludork-debug", "CMakeUserPresets.json")
SOURCE_IDE_FILES = ("generate_vs2022.bat", "generate_clion.bat", "generate_clion.sh")
EXPORT_DIRECTORIES = (
    "Scripts/Internal/UI", "Scripts/stub/Internal/UI", "Scripts/stub/Internal/UIWindows",
    "Scripts/stub/Enums", "Assets/Shaders/MaterialGenerated",
)
ANDROID_WRAPPER_FILES = (
    "gradlew", "gradle/wrapper/gradle-wrapper.jar", "gradle/wrapper/gradle-wrapper.properties",
    "gradle/wrapper/LICENSE.txt", "gradle/wrapper/NOTICE.md",
)
_ROOT_EXCLUDED_DIRECTORIES = (
    "Binaries", EDITOR_CACHE_DIRECTORY, "Cache", "Save", "Engine/ThirdParty/SFML/test",
    *EXPORT_DIRECTORIES,
)
_BUILD_DIRECTORY_NAMES = (
    ".venv", "build", "Intermediate", "bin", "Log", ".vs", ".idea",
    "cmake-build-ludork-debug", "ThirdPartySource",
)
_EXCLUDED_FILE_PATTERNS = (
    "*.anim.json", "*.py", "*.pyc", "*.pyo", "*.log", "BuildInfo.json", "Main.ini",
    "Main.plist", "Ludork.ini", "Ludork.plist", "CMakeUserPresets.json",
    "standalone-launch.json", "standalone-extensions.json",
)


def _present(path: pathlib.Path) -> bool:
    return os.path.lexists(path)


def _remove(path: pathlib.Path) -> None:
    if path.is_symlink() or path.is_file():
        remove_file(path)
    elif path.exists():
        remove_tree(path)


def _matches(value: str, pattern: str, platform: str) -> bool:
    if platform == "win32":
        value, pattern = value.casefold(), pattern.casefold()
    return fnmatch.fnmatchcase(value, pattern)


def is_source_entry_excluded(
    relative: pathlib.PurePosixPath,
    *,
    is_directory: bool,
    platform: str,
    include_ffmpeg: bool,
) -> bool:
    path = relative.as_posix()
    name = relative.name
    if platform != "win32" and name == ".DS_Store":
        return True
    locale_path = path.casefold() if platform == "win32" else path
    locale_prefix = "scripts/source/locale/" if platform == "win32" else "Scripts/Source/Locale/"
    if locale_path.startswith(locale_prefix):
        return locale_path != locale_prefix + ("core.lua" if platform == "win32" else "Core.lua")
    if is_directory:
        if _matches(name, "__pycache__", platform):
            return True
        if platform == "win32" and _matches(path, "Scripts/Source/Locale", platform):
            return True
        paths = _ROOT_EXCLUDED_DIRECTORIES
        if platform == "win32":
            paths += _BUILD_DIRECTORY_NAMES
            if not include_ffmpeg:
                paths += ("Engine/ThirdParty/ffmpeg",)
        elif name in _BUILD_DIRECTORY_NAMES or not include_ffmpeg and name == "ffmpeg":
            return True
        return any(_matches(path, excluded, platform) for excluded in paths)
    patterns = _EXCLUDED_FILE_PATTERNS + (
        ("generate_clion.sh",) if platform == "win32"
        else ("generate_vs2022.bat", "generate_clion.bat")
    )
    return any(_matches(name, pattern, platform) for pattern in patterns)


def copy_cpp_template(
    source: pathlib.Path,
    destination: pathlib.Path,
    *,
    platform: str,
    include_ffmpeg: bool,
    ffmpeg_version: str,
) -> None:
    def ignore(directory: str, names: list[str]) -> set[str]:
        parent = pathlib.Path(directory)
        relative = parent.relative_to(source)
        return {
            name for name in names
            if is_source_entry_excluded(
                pathlib.PurePosixPath((relative / name).as_posix()),
                is_directory=(parent / name).is_dir(),
                platform=platform,
                include_ffmpeg=include_ffmpeg,
            )
        }

    shutil.copytree(source, destination, dirs_exist_ok=True, symlinks=platform != "win32", ignore=ignore)
    if platform == "win32":
        locale_core = source / "Scripts/Source/Locale/Core.lua"
        if locale_core.is_file():
            target = destination / "Scripts/Source/Locale/Core.lua"
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(locale_core, target)
    if include_ffmpeg:
        archive = source / "ThirdPartySource" / f"ffmpeg-{ffmpeg_version}.tar.gz"
        (destination / "ThirdPartySource").mkdir(parents=True, exist_ok=True)
        shutil.copy2(archive, destination / "ThirdPartySource" / archive.name)


def copy_runtime_legal_files(
    repository: pathlib.Path,
    source: pathlib.Path,
    destination: pathlib.Path,
    *,
    platform: str,
    include_ffmpeg: bool,
) -> None:
    _remove(destination / "Licenses")
    copy_resources(repository, destination, "template-index")
    names = RUNTIME_LICENSE_DIRECTORIES + (("FFmpeg",) if include_ffmpeg else ())
    for name in names:
        shutil.copytree(
            repository / "Licenses" / name,
            destination / "Licenses" / name,
            symlinks=platform != "win32",
            ignore=shutil.ignore_patterns(".DS_Store") if platform != "win32" else None,
        )
    for name in RUNTIME_LEGAL_FILES:
        shutil.copy2(source / name, destination / name)


def copy_standalone_files(
    source_template: pathlib.Path,
    destination: pathlib.Path,
    original_source: pathlib.Path,
) -> None:
    (destination / ".vscode").mkdir(parents=True, exist_ok=True)
    files = (
        (source_template / ".vscode/settings.json", ".vscode/settings.json"),
        (original_source / ".vscode/standalone-launch.json", ".vscode/launch.json"),
        (original_source / ".vscode/standalone-extensions.json", ".vscode/extensions.json"),
        (source_template / ".emmyrc.json", ".emmyrc.json"),
        (source_template / ".gitignore", ".gitignore"),
    )
    for source, name in files:
        shutil.copy2(source, destination / name)


def copy_prebuilt_templates(source: pathlib.Path, destination: pathlib.Path, *, platform: str) -> None:
    source = source.resolve()
    target = destination.resolve()
    if source == target or source.is_relative_to(target) or target.is_relative_to(source):
        raise ValueError("Prepared template source and destination must not overlap")
    shutil.copytree(
        source, destination, dirs_exist_ok=True, symlinks=platform != "win32",
        ignore=shutil.ignore_patterns(".DS_Store") if platform != "win32" else None,
    )


def purge_template_runtime_state(
    templates: pathlib.Path,
    names: tuple[str, ...] = TEMPLATE_NAMES,
) -> None:
    for name in names:
        for relative in RUNTIME_STATE_PATHS:
            _remove(templates / name / relative)


def _require_file(path: pathlib.Path, *, executable: bool = False) -> None:
    if not path.is_file() or executable and not os.access(path, os.X_OK):
        raise ValueError(f"Required template {'executable' if executable else 'file'} was not found: {path}")


def _require_absent(path: pathlib.Path) -> None:
    if _present(path):
        raise ValueError(f"Forbidden template entry was found: {path}")


def _runtime_library(path: pathlib.Path) -> bool:
    return path.suffix.lower() in (".dll", ".so", ".dylib") or ".so." in path.name


def validate_standalone_runtime_layout(
    template: pathlib.Path,
    *,
    platform: str,
    verify_native: bool = True,
) -> None:
    windows = platform == "win32"
    main = template / ("Main.exe" if windows else "Main")
    _require_file(main, executable=not windows)
    binaries = template / "Binaries"
    if not binaries.is_dir():
        raise ValueError(f"Standalone runtime directory was not found: {binaries}")
    if windows:
        _require_file(binaries / "Main.exe")
    else:
        _require_absent(binaries / "Main")
    for path in template.iterdir():
        if (path.is_file() or path.is_symlink()) and _runtime_library(path):
            raise ValueError(f"Standalone runtime library exists outside Binaries: {path}")
    if not any(
        (path.is_file() or path.is_symlink())
        and (path.suffix.lower() == ".dll" if windows else path.suffix in (".so", ".dylib") or ".so." in path.name)
        for path in binaries.iterdir()
    ):
        raise ValueError(f"Standalone template contains no runtime libraries in Binaries: {template}")
    if verify_native:
        load_preview(template)
        if platform == "darwin":
            details = run(["otool", "-l", main], capture=True).stdout.decode("utf-8", errors="replace")
            if "path @loader_path/Binaries " not in details:
                raise ValueError(f"Standalone Main is missing the Binaries runtime search path: {main}")
            if "path @loader_path/../Frameworks " in details or "path @loader_path " in details:
                raise ValueError(f"Standalone Main contains an app-only or development runtime search path: {main}")
            run(["codesign", "--verify", "--strict", main])
    cache = template / EDITOR_CACHE_DIRECTORY
    if cache.is_dir():
        for path in cache.iterdir():
            if path.name not in ("Preview.json", "UiPreview.registry.json"):
                raise ValueError(f"Unexpected template {EDITOR_CACHE_DIRECTORY} entry: {path}")


def validate_templates(
    templates: pathlib.Path,
    *,
    platform: str,
    ffmpeg_version: str,
    names: tuple[str, ...] = TEMPLATE_NAMES,
    verify_native: bool = True,
) -> None:
    for name in names:
        if name not in TEMPLATE_NAMES:
            raise ValueError(f"Unknown project template: {name}")
        template = templates / name
        _require_file(template / "Main.proj")
        validate_ldpak_source(template)
        for relative in RUNTIME_LEGAL_FILES:
            _require_file(template / relative)
        for relative in RUNTIME_LICENSE_FILES:
            _require_file(template / "Licenses" / relative)
        for relative in ("Assets/Fonts/LICENSE.txt", "Assets/Musics/LICENSE.md"):
            _require_file(template / relative)
        for directory in NON_RUNTIME_LICENSE_DIRECTORIES:
            _require_absent(template / "Licenses" / directory)
        for relative in (*RUNTIME_STATE_PATHS, *EXPORT_DIRECTORIES, "Cache", "BuildInfo.json", "Data/BuildInfo.json"):
            _require_absent(template / relative)
        locale = template / "Scripts/Source/Locale"
        if locale.is_dir():
            for path in locale.iterdir():
                if path.name != "Core.lua":
                    raise ValueError(f"Generated locale output was found in a template: {path}")
        for relative in NATIVE_LUA_FILES:
            _require_file(template / "Scripts" / relative)
        _require_file(template / "Scripts/stub/Source/Configs/GeneralDataTypes.d.lua")
        for relative in NATIVE_ENUM_DIRECTORIES:
            if not (template / "Scripts" / relative).is_dir():
                raise ValueError(f"Native enum directory was not found: {template / 'Scripts' / relative}")
        if name.endswith("-ffmpeg"):
            for relative in FFMPEG_LICENSE_FILES:
                _require_file(template / "Licenses/FFmpeg" / relative)
            _require_file(template / "ThirdPartySource" / f"ffmpeg-{ffmpeg_version}.tar.gz")
        else:
            _require_absent(template / "Licenses/FFmpeg")
        if name in CPP_TEMPLATE_NAMES:
            for relative in ("Binaries", EDITOR_CACHE_DIRECTORY, "build", "bin", "Intermediate", *GENERATED_IDE_PATHS):
                _require_absent(template / relative)
            _require_file(template / "Engine/PreviewHost/CMakeLists.txt")
            wrapper_files = ANDROID_WRAPPER_FILES + (("gradlew.bat",) if platform == "win32" else ())
            for relative in wrapper_files:
                _require_file(template / "Engine/PlatformHosts/Android" / relative)
            if platform == "win32":
                for relative in SOURCE_IDE_FILES[:2]:
                    _require_file(template / relative)
                _require_absent(template / "generate_clion.sh")
            else:
                _require_file(template / "generate_clion.sh", executable=True)
                for relative in SOURCE_IDE_FILES[:2]:
                    _require_absent(template / relative)
        else:
            for relative in SOURCE_IDE_FILES:
                _require_absent(template / relative)
            validate_standalone_runtime_layout(template, platform=platform, verify_native=verify_native)
