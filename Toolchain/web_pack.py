from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import sys

from . import server_config
from .compile_lua import resolve_luac
from .pack_error import PackError
from .package_process import run
from .package_resources import prepare_runtime_resources
from .package_staging import package_staging
from .packaging_constants import EXIT_PROJECT, EXIT_TOOLCHAIN, RUNTIME_LEGAL_FILES
from .packaging_metadata import add_package_arguments, read_package_metadata
from .preview import prepare_registry
from .template_contents import copy_runtime_legal_files
from .tool_paths import resolve_toolchain_directory
from .tool_process import cancellation_signals
from .ui_asset_generation import generate_assets
from .ui_property_values import UiAssetError
from .web_toolchain import ffmpeg_tools, resolve_web_toolchain


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ludork-pack web-pack")
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("dist", type=pathlib.Path, nargs="?")
    parser.add_argument("--tools-directory", type=pathlib.Path)
    parser.add_argument("--emsdk", type=pathlib.Path)
    parser.add_argument("--check", action="store_true")
    for name in ("compile-lua", "encrypt-shaders", "encrypt-data", "encrypt-saves", "use-ldpak"):
        parser.add_argument("--" + name, action="store_true")
    add_package_arguments(parser)
    return parser


def _ordinary_path(path: pathlib.Path) -> None:
    for parent in (path, *path.parents):
        if parent.is_symlink() or parent.is_junction():
            raise PackError(f"Web package paths must not contain links: {parent}", EXIT_PROJECT)


def _validate_output(project: pathlib.Path, destination: pathlib.Path) -> None:
    _ordinary_path(destination)
    if project == destination or project.is_relative_to(destination):
        raise PackError("Web package output must not replace the source project or an ancestor.", EXIT_PROJECT)
    if destination.is_relative_to(project) and not destination.is_relative_to(project / "dist"):
        raise PackError("An output inside the source project must be below dist.", EXIT_PROJECT)
    if destination.exists() and not destination.is_dir():
        raise PackError(f"Web package output is not a directory: {destination}", EXIT_PROJECT)


def _validate_project(project: pathlib.Path) -> bool:
    _ordinary_path(project)
    data = json.loads((project / "Main.proj").read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict) or data.get("Cpp") is not True:
        raise PackError("WebGL packaging requires a C++ Source project.", EXIT_PROJECT)
    required = ("CMakeLists.txt", "Scripts/Entry.lua", "Data/Configs/System.json",
                "Engine/PlatformHosts/Web/index.html", "Engine/cmake/LudorkWebApplication.cmake",
                "Engine/ThirdParty/SFML/src/SFML/Window/Emscripten/WindowImplWeb.cpp",
                "Engine/ThirdParty/LuaSF/CMakeLists.txt", "Engine/ThirdParty/LuaGlue/CMakeLists.txt",
                "Engine/ThirdParty/Lua/src/lua.h", "Engine/ThirdParty/lua-cjson/lua_cjson.c",
                "Engine/ThirdParty/zlib/CMakeLists.txt")
    for relative in required:
        if not (project / relative).is_file():
            raise PackError(f"Web project input is missing: {relative}. Update project sources and dependencies explicitly before packaging.", EXIT_PROJECT)
    for name in ("Assets", "Data", "Scripts"):
        root = project / name
        if not root.is_dir():
            raise PackError(f"Project resource directory is missing: {root}", EXIT_PROJECT)
        for path in (root, *root.rglob("*")):
            if path.is_symlink() or path.is_junction():
                raise PackError(f"Web package resources must not contain links: {path}", EXIT_PROJECT)
    luasf = (project / "Engine/ThirdParty/LuaSF/CMakeLists.txt").read_text(encoding="utf-8-sig")
    if "EMSCRIPTEN" not in luasf and "Emscripten" not in luasf:
        raise PackError("The installed LuaSF source does not support Web. Import a current source archive explicitly.", EXIT_PROJECT)
    enabled = data.get("ffmpeg") is True
    if enabled:
        for relative in ("Engine/ThirdParty/ffmpeg/configure", "Engine/cmake/FFmpeg/build_web.sh"):
            if not (project / relative).is_file():
                raise PackError(f"FFmpeg Web build input is missing: {relative}", EXIT_PROJECT)
    return enabled


def _copy_legal(project: pathlib.Path, destination: pathlib.Path, ffmpeg: bool,
                repository: pathlib.Path | None = None) -> None:
    licenses = project / "Licenses"
    if repository is not None:
        copy_runtime_legal_files(repository, project, destination, platform=sys.platform, include_ffmpeg=ffmpeg)
    elif licenses.is_dir():
        shutil.copytree(licenses, destination / "Licenses")
    for name in RUNTIME_LEGAL_FILES:
        source = project / name
        if source.is_file():
            shutil.copy2(source, destination / name)
    if ffmpeg and (project / "ThirdPartySource").is_dir():
        shutil.copytree(project / "ThirdPartySource", destination / "ThirdPartySource")
        build_scripts = project / "Engine/cmake/FFmpeg"
        shutil.copytree(build_scripts, destination / "ThirdPartySource/FFmpeg-Build", dirs_exist_ok=True)


def pack(args: argparse.Namespace) -> pathlib.Path:
    if sys.platform not in ("win32", "darwin"):
        raise PackError("WebGL packaging supports Windows and macOS hosts.", EXIT_TOOLCHAIN)
    project = pathlib.Path(os.path.abspath(args.project.expanduser()))
    ffmpeg = _validate_project(project)
    metadata = read_package_metadata(project, args.version, args.dev)
    dist = pathlib.Path(os.path.abspath(args.dist.expanduser())) if args.dist else project / "dist"
    destination = dist / (metadata.package_name + "-webgl")
    _validate_output(project, destination)
    sdk = resolve_web_toolchain(args.emsdk)
    toolchain = resolve_toolchain_directory()
    tools = args.tools_directory.resolve() if args.tools_directory else toolchain.parent
    repository = (tools.parent if (tools.parent / "Ludork.csproj").is_file()
                  and project == tools.parent / "Game" else None)
    native_tools = ffmpeg_tools(sdk, tools) if ffmpeg else None
    server_config.load("Release", target="web")
    if args.compile_lua or os.environ.get("LUDORK_PACK_COMPILE_LUA_DIRECTORIES", ""):
        resolve_luac()
    if args.check:
        print("WebGL project and Emscripten prerequisites are ready.", flush=True)
        return destination
    preview_environment = os.environ.copy()
    preview_environment["LUDORK_SERVER_ENABLED"] = "0"
    for name in ("LUDORK_SERVER_URL", "LUDORK_SERVER_KEY", "LUDORK_SERVER_CONFIG_FILE"):
        preview_environment.pop(name, None)
    registry = prepare_registry(project, toolchain, environment=preview_environment)
    changes = generate_assets(project, check=True)
    if changes:
        raise PackError("UI export is missing or outdated. Run editor Export before packaging:\n" + "\n".join(changes), EXIT_PROJECT)
    build = project / "build-web"
    _ordinary_path(build)
    resources = build / "resources"
    prepare_runtime_resources(project, resources, metadata=metadata, registry=registry, licenses=False,
                              compile_lua=args.compile_lua, encrypt_shaders=args.encrypt_shaders,
                              encrypt_data=args.encrypt_data, use_ldpak=args.use_ldpak)
    output = build / "bin" / "Release"
    configure = [sdk.cmake, "-S", project, "-B", build, "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_MAKE_PROGRAM={sdk.ninja.as_posix()}",
                 f"-DCMAKE_TOOLCHAIN_FILE={(sdk.emscripten / 'cmake/Modules/Platform/Emscripten.cmake').as_posix()}",
                 f"-DLUDORK_TOOLCHAIN_DIRECTORY={toolchain.as_posix()}",
                 "-DLUDORK_BUILD_PREVIEW_HOST=OFF", f"-DLUDORK_UI_REGISTRY_PATH={registry.as_posix()}",
                 f"-DLUDORK_WEB_RESOURCE_DIRECTORY={resources.as_posix()}",
                 f"-DLUDORK_RUNTIME_OUTPUT_DIRECTORY={(build / 'bin').as_posix()}",
                 f"-DLUDORK_SAVE_AS_LDC={'ON' if args.encrypt_saves else 'OFF'}",
                 f"-DLUDORK_WEB_APP_NAME={metadata.app_name}"]
    if native_tools:
        shell, make, host_cc = native_tools
        configure.extend([f"-DLUDORK_WEB_SHELL={shell.as_posix()}",
                          f"-DLUDORK_GNU_MAKE_EXECUTABLE={make.as_posix()}",
                          f"-DLUDORK_FFMPEG_HOST_CC={host_cc}"])
    try:
        print("Building the WebGL runtime…", flush=True)
        run(configure, cwd=project, env=sdk.environment)
        run([sdk.cmake, "--build", build, "--target", "Main", "--parallel",
             sdk.environment.get("CMAKE_BUILD_PARALLEL_LEVEL", str(os.cpu_count() or 1))],
            cwd=project, env=sdk.environment)
        required = ("index.html", "Main.js", "Main.wasm", "Main.data")
        for name in required:
            if not (output / name).is_file() or (output / name).stat().st_size == 0:
                raise PackError(f"The Web build did not produce {output / name}.")
        if (output / "Main.wasm").read_bytes()[:4] != b"\0asm":
            raise PackError("The Web runtime output is not a WebAssembly module.")
        with package_staging(destination) as staging:
            staging.mkdir()
            for name in required:
                shutil.copy2(output / name, staging / name)
            _copy_legal(project, staging, ffmpeg, repository)
    finally:
        server_config.cleanup(project)
    print(f"WebGL package complete: {destination}", flush=True)
    return destination


def main(arguments: list[str] | None = None) -> int:
    args = create_parser().parse_args(arguments)
    try:
        with cancellation_signals():
            pack(args)
        return 0
    except KeyboardInterrupt:
        print("WebGL packaging cancelled.", file=sys.stderr, flush=True)
        return 130
    except PackError as error:
        print(f"Error: {error}", file=sys.stderr, flush=True)
        return error.exit_code
    except (OSError, ValueError, RuntimeError, UiAssetError) as error:
        print(f"Error: {error}", file=sys.stderr, flush=True)
        return EXIT_PROJECT
