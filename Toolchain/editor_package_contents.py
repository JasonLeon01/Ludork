from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import sys
import tarfile
from fnmatch import fnmatchcase

from .editor_build_metadata import validate_payload
from .editor_official_plugins import validate as validate_plugins
from .file_replace import remove_file, remove_tree
from .pack_error import PackError
from .packaging_metadata import load_release_version
from .prune_editor_macos_publish import validatePrunedData
from .runtime_bundle import validate_bundle


EDITOR_LICENSE_PATHS = (
    "DotNet/LICENSE.txt", "DotNet/THIRD-PARTY-NOTICES.txt",
    "DotNetPackages/Microsoft.Extensions-8.0.0-LICENSE.txt",
    "DotNetPackages/Microsoft.Extensions-8.0.0-THIRD-PARTY-NOTICES.txt",
    "DotNetPackages/Microsoft.Win32.Registry-and-System.Security-4.7.0-LICENSE.txt",
    "DotNetPackages/Microsoft.Win32.Registry-and-System.Security-4.7.0-THIRD-PARTY-NOTICES.txt",
    "DotNetPackages/System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-LICENSE.txt",
    "DotNetPackages/System.Collections.Immutable-and-System.Reflection.Metadata-10.0.1-THIRD-PARTY-NOTICES.txt",
    "DotNetPackages/System.IO.Pipelines-8.0.0-LICENSE.txt",
    "DotNetPackages/System.IO.Pipelines-8.0.0-THIRD-PARTY-NOTICES.txt",
    "DotNetPackages/System.Memory-4.5.3-LICENSE.txt",
    "DotNetPackages/System.Memory-4.5.3-THIRD-PARTY-NOTICES.txt",
    "DotNetPackages/System.ValueTuple-4.5.0-LICENSE.txt",
    "DotNetPackages/System.ValueTuple-4.5.0-THIRD-PARTY-NOTICES.txt",
    "README.md", "README_zh_CN.md", "Avalonia/ANGLE-LICENSE.txt", "Avalonia/LICENSE.txt",
    "Avalonia/Inter-OFL-1.1.txt", "EditorPackages/AvaloniaEdit-LICENSE.txt",
    "EditorPackages/Avalonia.Controls.WebView-LICENSE.txt",
    "EditorPackages/CommunityToolkit.Mvvm-LICENSE.md", "EditorPackages/Svg.Skia-LICENSE.txt",
    "EditorPackages/Svg.Custom-LICENSE.txt", "EditorPackages/ExCSS-LICENSE.txt",
    "EditorPackages/CommunityToolkit.Mvvm-THIRD-PARTY-NOTICES.txt",
    "EditorPackages/HarfBuzzSharp-LICENSE.txt", "EditorPackages/MoonSharp-LICENSE.txt",
    "EditorPackages/NAudio-LICENSE.txt", "EditorPackages/NAudio.Vorbis-LICENSE.txt",
    "EditorPackages/NVorbis-LICENSE.txt", "EditorPackages/NodifyM.Avalonia-LICENSE.txt",
    "EditorPackages/Roslyn-LICENSE.txt", "EditorPackages/Roslyn-THIRD-PARTY-NOTICES.rtf",
    "EditorPackages/SkiaSharp-LICENSE.txt", "EditorPackages/SharpCompress-LICENSE.txt",
    "EditorPackages/System.Reactive-LICENSE.txt", "EditorPackages/MicroCom.Runtime-LICENSE.txt",
    "EditorPackages/Microsoft.IO.RecyclableMemoryStream-LICENSE.txt",
    "EditorPackages/SkiaSharp-and-HarfBuzzSharp-NativeAssets-THIRD-PARTY-NOTICES.txt",
    "EditorPackages/Tmds.DBus.Protocol-LICENSE.txt", "LuaSF/LICENSE.txt", "Lua/LICENSE.txt",
    "SFML/LICENSE.txt", "LuaGlue/LICENSE", "lua-cjson/LICENSE.txt", "zlib/LICENSE.txt",
    "IXWebSocket/LICENSE.txt", "IXWebSocket/THIRD-PARTY.txt", "FFmpeg/COPYING.GPLv2.txt",
    "FFmpeg/COPYING.GPLv3.txt", "FFmpeg/COPYING.LGPLv2.1.txt", "FFmpeg/COPYING.LGPLv3.txt",
    "FFmpeg/README.md", "FFmpeg/UPSTREAM-LICENSE.md", "GNUMake/COPYING.txt",
    "MicrosoftVisualCppRuntime/README.md",
    "MicrosoftVisualCppRuntime/Visual-C-Runtime-2015-2022-License.docx",
    "NativeDependencies/FreeType-FTL.txt", "NativeDependencies/FreeType-LICENSE.txt",
    "NativeDependencies/Glad-CC0-1.0.txt", "NativeDependencies/HarfBuzz-COPYING.txt",
    "NativeDependencies/SheenBidi-LICENSE.txt", "NativeDependencies/Ogg-COPYING.txt",
    "NativeDependencies/Vorbis-COPYING.txt", "NativeDependencies/Wine-DInput-LGPLv2.1.txt",
    "NativeDependencies/FLAC-COPYING.Xiph.txt", "NativeDependencies/MbedTLS-LICENSE.txt",
    "NativeDependencies/libssh2-COPYING.txt", "NativeDependencies/SFML-THIRD-PARTY.md",
    "HarmonyOSSans/LICENSE.txt", "SampleMusic/NOTICE.md",
    "ScriptTools/Nuitka-4.1.3-AGPL-3.0.txt", "ScriptTools/Nuitka-4.1.3-NOTICE.txt",
    "ScriptTools/Nuitka-4.1.3-RUNTIME-EXCEPTION.txt",
    "ScriptTools/Python-3.12-LICENSES-AND-ACKNOWLEDGEMENTS.rst.txt",
    "ScriptTools/Pillow-12.2.0-LICENSE.txt",
)

BUILD_TOOL_SOURCES = {
    "windows": {
        "build_cpp.bat": "tools/editor_runtime/build_cpp.bat",
        **{name: "tools/" + name for name in (
            "build_standalone.bat", "build_preview_host.bat", "pack_project.bat", "pack_standalone.bat",
        )},
    },
    "macos": {
        **{name: "tools/" + name for name in (
            "common.sh", "build_standalone.sh", "pack_standalone.sh", "build_preview_host.sh", "pack_project.sh",
        )},
        **{name: "tools/editor_runtime/" + name for name in (
            "build_cpp.sh", "pack_ios.sh", "pack_harmony.sh", "pack_android.sh", "unregister_editor.sh",
        )},
    },
}

WINDOWS_RUNTIME_FILES = (
    "Ludork.exe", "Binaries/Ludork.exe", "Binaries/Ludork.dll", "Binaries/Ludork.deps.json",
    "Binaries/Ludork.runtimeconfig.json", "Binaries/coreclr.dll", "Binaries/hostfxr.dll",
    "Binaries/hostpolicy.dll", "Binaries/System.Private.CoreLib.dll",
)
USER_STATE = ("Ludork.ini", "Ludork.plist", "*-signing.json", ".localserver", "Log", "WebView2")
PUBLIC_DOCUMENTATION = ("docs", "About_*.md", "THIRD_PARTY_NOTICES*.md")
SERVER_CONTENT = ("Server", "LudorkServer*.tar.gz")
BUILD_ONLY_TOOLS = (
    "pack_editor.sh", "pack_editor.bat", "pack_editor_msi.bat", "pack_editor_exe.bat",
    "validate_editor_windows_layout.bat", "editor_launcher", "installer",
    "create_templates.sh", "create_templates.bat", "run_editor.sh", "run_editor.bat",
)


def require_file(path: pathlib.Path, *, executable: bool = False) -> None:
    if not path.is_file() or (executable and not os.access(path, os.X_OK)):
        raise PackError(f"Required package {'executable' if executable else 'file'} was not found: {path}")


def require_directory(path: pathlib.Path) -> None:
    if not path.is_dir():
        raise PackError(f"Required package directory was not found: {path}")


def remove_path(path: pathlib.Path) -> None:
    if path.is_symlink() or path.is_file():
        remove_file(path)
    elif path.is_dir():
        remove_tree(path)


def copy_directory(source: pathlib.Path, destination: pathlib.Path) -> None:
    require_directory(source)
    if source.resolve() == destination.resolve():
        raise PackError(f"Package copy source and destination must differ: {source}")
    remove_path(destination)
    shutil.copytree(source, destination, symlinks=True, ignore=shutil.ignore_patterns(".DS_Store"))


def reject_paths(root: pathlib.Path, patterns: tuple[str, ...]) -> None:
    for pattern in patterns:
        for path in root.glob(pattern):
            raise PackError(f"Forbidden package entry was found: {path}")


def roots(package: pathlib.Path, platform: str) -> tuple[pathlib.Path, pathlib.Path]:
    if platform == "macos":
        return package / "Contents/MacOS", package / "Contents/Resources"
    return package / "Binaries", package


def stage_build_tools(repository: pathlib.Path, destination: pathlib.Path,
                      platform: str, versions: dict[str, str]) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    for name, source in BUILD_TOOL_SOURCES[platform].items():
        require_file(repository / source)
        shutil.copy2(repository / source, destination / name)
        if platform == "macos" and name != "common.sh":
            (destination / name).chmod((destination / name).stat().st_mode | 0o111)
    bundle = repository / ".tools/Toolchain"
    validate_bundle(bundle)
    copy_directory(bundle, destination / "Toolchain")
    luac_name = "luac.exe" if platform == "windows" else "luac"
    luac = repository / ".tools/Lua" / luac_name
    require_file(luac)
    shutil.copy2(luac, destination / luac_name)
    if platform == "macos":
        (destination / luac_name).chmod((destination / luac_name).stat().st_mode | 0o111)
    else:
        make_version = versions["GNU_MAKE_VERSION"]
        make = destination / "gnu-make"
        make.mkdir(exist_ok=True)
        source = repository / f".tools/sources/make-{make_version}.tar.gz"
        shutil.copy2(repository / ".tools/gnu-make/gnumake.exe", make / "gnumake.exe")
        shutil.copy2(source, make / source.name)
        license_source = repository / f".tools/build/make-{make_version}/COPYING"
        if license_source.is_file():
            shutil.copy2(license_source, make / "COPYING")
        else:
            with tarfile.open(source, "r:gz") as archive:
                stream = archive.extractfile(f"make-{make_version}/COPYING")
                if stream is None:
                    raise PackError(f"GNU Make source archive is missing COPYING: {source}")
                with stream, (make / "COPYING").open("wb") as output:
                    shutil.copyfileobj(stream, output)


def clean_package(package: pathlib.Path, platform: str, *, templates: bool) -> None:
    patterns = ("*.pyc", "*.pyo", "*.pdb") + (("*.bat", ".DS_Store") if platform == "macos" else ())
    for path in sorted(package.rglob("*"), key=lambda item: len(item.parts), reverse=True):
        if path.name == "__pycache__" or any(fnmatchcase(path.name, pattern) for pattern in patterns):
            remove_path(path)
    if templates:
        from .template_contents import purge_template_runtime_state
        purge_template_runtime_state(roots(package, platform)[1] / "Templates")


def validate_layout(package: pathlib.Path, platform: str, channel: str) -> None:
    binaries, content = roots(package, platform)
    require_directory(package)
    if platform == "windows":
        for name in WINDOWS_RUNTIME_FILES:
            require_file(package / name)
        if channel == "release":
            reject_paths(package, ("Plugins", "plugins.json"))
        else:
            reject_paths(package, ("BuildTools/OfficialPlugins",))
        reject_paths(package, (*SERVER_CONTENT, *PUBLIC_DOCUMENTATION, *USER_STATE,
                               "*.dll", "*.so", "*.dylib", "*.deps.json", "*.runtimeconfig*.json", "tools",
                               *("BuildTools/" + name for name in ("installer", "pack_editor_msi.bat", "pack_editor_exe.bat"))))
        for path in package.glob("*.exe"):
            if path.name.casefold() != "ludork.exe":
                raise PackError(f"Unexpected executable at the editor package root: {path}")
        reject_paths(binaries, (*SERVER_CONTENT, *PUBLIC_DOCUMENTATION, *USER_STATE,
                                "Locale", "Templates", "tools", "BuildTools", "Plugins", "plugins.json",
                                "Licenses", "LICENSE.md", "README*.md", ".ludork-development"))
    else:
        require_file(binaries / "Ludork", executable=True)
        require_file(package / "Contents/Info.plist")
        require_directory(content)


def validate_package(package: pathlib.Path, *, platform: str, channel: str, templates: bool,
                     build_info: pathlib.Path, gnu_make_version: str = "", ffmpeg_version: str = "",
                     verify_native: bool = True) -> None:
    validate_layout(package, platform, channel)
    binaries, content = roots(package, platform)
    if load_release_version(build_info).dev != (channel == "dev"):
        raise PackError("Editor package channel does not match its build information.")
    validate_payload(package, build_info, content)
    for name in ("Locale/en_GB", "Locale/zh_CN", "LICENSE.md", "README.md", "README_zh_CN.md"):
        require_file(content / name)
    require_directory(content / "Licenses")
    for name in EDITOR_LICENSE_PATHS:
        require_file(content / "Licenses" / name)
    tools = content / "BuildTools"
    for name in BUILD_TOOL_SOURCES[platform]:
        require_file(tools / name, executable=platform == "macos" and name != "common.sh")
    require_file(tools / ("luac.exe" if platform == "windows" else "luac"), executable=platform == "macos")
    validate_bundle(tools / "Toolchain")
    if platform == "windows":
        for name in ("gnumake.exe", f"make-{gnu_make_version}.tar.gz", "COPYING"):
            require_file(tools / "gnu-make" / name)
        validate_plugins(content / "BuildTools/OfficialPlugins" if channel == "release" else content)
        from .prune_editor_windows_publish import FOREIGN_ASSEMBLIES, FOREIGN_PACKAGES
        require_file(binaries / "Avalonia.Metal.dll")
    else:
        from .prune_editor_macos_publish import FOREIGN_ASSEMBLIES, FOREIGN_PACKAGES
        for name in ("AppIcon.icns", "ProjectIcon.icns"):
            require_file(content / name)
        for path in package.rglob("*"):
            if path.name in ("Plugins", "plugins.json"):
                raise PackError(f"Plugin payload was found inside the editor app: {path}")
    reject_paths(binaries, tuple(FOREIGN_ASSEMBLIES))
    manifest = json.loads((binaries / "Ludork.deps.json").read_text(encoding="utf-8"))
    validatePrunedData(manifest, FOREIGN_PACKAGES, FOREIGN_ASSEMBLIES)
    if platform == "windows" and "Avalonia.Metal" not in json.dumps(manifest):
        raise PackError("Required Avalonia.Metal dependency was not found in Ludork.deps.json.")
    reject_paths(content, (*SERVER_CONTENT, *PUBLIC_DOCUMENTATION, *USER_STATE,
                           "Page", "Locale/locale.json", "Game", ".ludork-development", "requirements.txt",
                           "versions.conf", ".venv", ".tools",
                           *("BuildTools/" + name for name in BUILD_ONLY_TOOLS)))
    if platform == "macos":
        reject_paths(binaries, (*SERVER_CONTENT, *PUBLIC_DOCUMENTATION, *USER_STATE,
                                "Game", ".ludork-development", "requirements.txt", "versions.conf", ".venv", ".tools",
                                "LICENSE.md", "README.md", "README_zh_CN.md", "Locale", "Templates", "Page", "Licenses", "tools"))
    forbidden = ("__pycache__", "*.py", "*.pyc", "*.pyo", "*.pdb", "*.resources.dll")
    if platform == "macos":
        forbidden += ("*.bat", ".DS_Store")
    for path in package.rglob("*"):
        if any(fnmatchcase(path.name, pattern) for pattern in forbidden):
            raise PackError(f"Forbidden package entry was found: {path}")
        if platform == "windows" and path.is_relative_to(tools) and path.suffix == ".sh":
            raise PackError(f"Non-Windows tool was found: {path}")
    if templates:
        from .template_contents import validate_templates
        validate_templates(content / "Templates", platform="win32" if platform == "windows" else "darwin",
                           ffmpeg_version=ffmpeg_version, verify_native=verify_native)
    else:
        reject_paths(content, ("Templates",))
    if platform == "macos" and verify_native:
        from .editor_package_macos import validate_native_package
        validate_native_package(package, build_info)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ludork-editor package-validate")
    parser.add_argument("root", type=pathlib.Path)
    parser.add_argument("--platform", choices=("windows", "macos"), required=True)
    parser.add_argument("--channel", choices=("dev", "release"), required=True)
    parser.add_argument("--phase", choices=("layout", "full"), default="full")
    parser.add_argument("--build-info", type=pathlib.Path)
    parser.add_argument("--templates", action="store_true")
    parser.add_argument("--gnu-make-version", default="")
    parser.add_argument("--ffmpeg-version", default="")
    parsed = parser.parse_args(arguments)
    try:
        if parsed.phase == "layout":
            validate_layout(parsed.root, parsed.platform, parsed.channel)
        else:
            if parsed.build_info is None:
                raise PackError("Full package validation requires --build-info.")
            validate_package(parsed.root, platform=parsed.platform, channel=parsed.channel,
                             templates=parsed.templates, build_info=parsed.build_info,
                             gnu_make_version=parsed.gnu_make_version, ffmpeg_version=parsed.ffmpeg_version)
        return 0
    except (OSError, ValueError, RuntimeError, tarfile.TarError) as error:
        print(str(error), file=sys.stderr)
        return error.exit_code if isinstance(error, PackError) else 1
