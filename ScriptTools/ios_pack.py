from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import plistlib
import re
import shutil
import sys
import unicodedata
import zipfile

from .compile_lua import compile_scripts, resolve_luac
from .finalize_package import finalize_package
from .ios_device import device_identifier
from .ios_device import install_and_launch as install_and_launch_on_device
from .ios_device import require_device_tools
from .ios_device import requires_developer_trust
from .ios_device import select_iphone
from .ios_toolchain import EXIT_APP_NAME_UNCHANGED
from .ios_toolchain import EXIT_PROJECT
from .ios_toolchain import EXIT_TOOLCHAIN
from .ios_toolchain import PackError
from .ios_toolchain import choose_team_id
from .ios_toolchain import require_cmake
from .ios_toolchain import require_xcode_tools
from .ios_toolchain import resolve_cmake
from .ios_toolchain import resolve_developer_dir
from .ios_toolchain import run_capture
from .ios_toolchain import run_streaming
from .ios_toolchain import select_team_id
from .ios_toolchain import xcode_account_team_ids


DEFAULT_APP_NAME_PATTERN = re.compile(
    r"""^[ \t]*local[ \t]+APP_NAME[ \t]*=[ \t]*["']LudorkSample["'][ \t]*(?:--[^\r\n]*)?\r?$""",
    re.MULTILINE,
)


class PackContext:
    def __init__(
        self,
        project_dir: pathlib.Path,
        dist_dir: pathlib.Path,
        developer_dir: pathlib.Path,
        cmake: pathlib.Path,
        team_id: str,
        game_name: str,
        artifact_name: str,
        bundle_identifier: str,
        use_luac: bool,
        encrypt_shaders: bool,
        encrypt_data: bool,
    ) -> None:
        self.project_dir = project_dir
        self.dist_dir = dist_dir
        self.developer_dir = developer_dir
        self.cmake = cmake
        self.team_id = team_id
        self.game_name = game_name
        self.artifact_name = artifact_name
        self.bundle_identifier = bundle_identifier
        self.use_luac = use_luac
        self.encrypt_shaders = encrypt_shaders
        self.encrypt_data = encrypt_data

    @property
    def environment(self) -> dict[str, str]:
        environment = os.environ.copy()
        environment["DEVELOPER_DIR"] = str(self.developer_dir)
        return environment


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="pack_ios",
        usage="pack_ios [--check] [--compile-lua] [--encrypt-shaders] [--encrypt-data] [--export-to-iphone] <project-folder> [dist-folder]",
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--compile-lua", action="store_true")
    parser.add_argument("--encrypt-shaders", action="store_true")
    parser.add_argument("--encrypt-data", action="store_true")
    parser.add_argument("--export-to-iphone", action="store_true")
    parser.add_argument("project_folder")
    parser.add_argument("dist_folder", nargs="?")
    return parser.parse_args(arguments)


def resolve_project(project_folder: str) -> pathlib.Path:
    project_dir = pathlib.Path(project_folder).expanduser().resolve()
    if not project_dir.is_dir():
        raise PackError(
            f"Project folder was not found: {project_dir}",
            EXIT_PROJECT,
        )
    project_file = project_dir / "Main.proj"
    cmake_file = project_dir / "CMakeLists.txt"
    if not project_file.is_file():
        raise PackError(f"Main.proj was not found: {project_file}", EXIT_PROJECT)
    try:
        project_data = json.loads(project_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise PackError(f"Unable to read Main.proj: {exception}", EXIT_PROJECT) from exception
    if not isinstance(project_data, dict) or project_data.get("Cpp") is not True:
        raise PackError(
            "iOS packaging requires a C++ project. Standalone desktop projects are not supported.",
            EXIT_PROJECT,
        )
    if not cmake_file.is_file():
        raise PackError(f"CMakeLists.txt was not found: {cmake_file}", EXIT_PROJECT)
    for directory_name in (
        "Assets",
        "Core",
        "Data",
        "include",
        "LuaSF",
        "lua-cjson",
        "Scripts",
        "src",
        "Standard",
        "zlib",
    ):
        directory = project_dir / directory_name
        if not directory.is_dir():
            raise PackError(
                f"Required iOS project folder was not found: {directory}",
                EXIT_PROJECT,
            )
    entry_path = project_dir / "Scripts" / "Entry.lua"
    if not entry_path.is_file():
        raise PackError(f"Lua entry script was not found: {entry_path}", EXIT_PROJECT)
    try:
        entry_source = entry_path.read_text(encoding="utf-8")
    except OSError as exception:
        raise PackError(
            f"Unable to read Lua entry script: {exception}",
            EXIT_PROJECT,
        ) from exception
    if DEFAULT_APP_NAME_PATTERN.search(entry_source):
        raise PackError(
            "Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging.",
            EXIT_APP_NAME_UNCHANGED,
        )
    system_assets = project_dir / "Assets" / "System"
    if not any(
        (system_assets / icon_name).is_file()
        for icon_name in ("icon.icns", "icon.png")
    ):
        raise PackError(
            f"Project icon was not found in {system_assets}.",
            EXIT_PROJECT,
        )
    if project_data.get("ffmpeg") is True:
        ffmpeg_configure = project_dir / "ffmpeg" / "configure"
        ffmpeg_cmake = project_dir / "cmake" / "FFmpeg" / "CMakeLists.txt"
        if not ffmpeg_configure.is_file() or not ffmpeg_cmake.is_file():
            raise PackError(
                "The project enables FFmpeg but its iOS build sources are incomplete.",
                EXIT_PROJECT,
            )
    return project_dir


def read_game_name(project_dir: pathlib.Path) -> str:
    system_path = project_dir / "Data" / "Configs" / "System.json"
    try:
        data = json.loads(system_path.read_text(encoding="utf-8"))
        title = data["title"]["value"]
    except (OSError, json.JSONDecodeError, KeyError, TypeError) as exception:
        raise PackError(
            f"Unable to read game title from {system_path}: {exception}",
            EXIT_PROJECT,
        ) from exception
    if not isinstance(title, str) or not title.strip():
        raise PackError(
            f"Game title must be a non-empty string: {system_path}",
            EXIT_PROJECT,
        )
    return title


def artifact_name(game_name: str) -> str:
    normalized = unicodedata.normalize("NFC", game_name)
    safe = re.sub(r"[\x00-\x1f\x7f<>:\"/\\|?*;]+", "-", normalized)
    safe = re.sub(r"\s+", " ", safe).strip(" .")
    if not safe:
        safe = "Ludork Game"
    return safe[:80].rstrip(" .") or "Ludork Game"


def bundle_identifier(team_id: str, game_name: str) -> str:
    ascii_name = (
        unicodedata.normalize("NFKD", game_name)
        .encode("ascii", "ignore")
        .decode("ascii")
        .lower()
    )
    slug = re.sub(r"[^a-z0-9]+", "-", ascii_name).strip("-")[:40] or "game"
    digest = hashlib.sha256(game_name.encode("utf-8")).hexdigest()[:10]
    return f"com.ludork.{team_id.lower()}.{slug}.{digest}"


def create_context(arguments: argparse.Namespace) -> PackContext:
    if sys.platform != "darwin":
        raise PackError("iOS packaging is only supported on macOS.", EXIT_TOOLCHAIN)
    project_dir = resolve_project(arguments.project_folder)
    dist_dir = (
        pathlib.Path(arguments.dist_folder).expanduser().resolve()
        if arguments.dist_folder
        else project_dir / "dist"
    )
    developer_dir = resolve_developer_dir()
    cmake = resolve_cmake()
    cmake_version = require_cmake(cmake)
    game_name = read_game_name(project_dir)
    tools = require_xcode_tools(developer_dir)
    team_id = select_team_id()
    name = artifact_name(game_name)
    identifier = bundle_identifier(team_id, game_name)
    if arguments.compile_lua:
        luac = resolve_luac()
        print(f"luac: {luac}")
    print(f"Xcode: {tools['xcodebuild'].splitlines()[0]}")
    print(f"CMake: {cmake_version}")
    print(f"Developer directory: {developer_dir}")
    print(f"Signing team: {team_id}")
    print(f"Game name: {game_name}")
    print(f"Bundle identifier: {identifier}")
    return PackContext(
        project_dir,
        dist_dir,
        developer_dir,
        cmake,
        team_id,
        game_name,
        name,
        identifier,
        arguments.compile_lua,
        arguments.encrypt_shaders,
        arguments.encrypt_data,
    )


def write_info_plist(context: PackContext, path: pathlib.Path) -> None:
    data = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": context.game_name,
        "CFBundleExecutable": "$(EXECUTABLE_NAME)",
        "CFBundleIdentifier": context.bundle_identifier,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": context.artifact_name,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "1.0.0",
        "CFBundleVersion": "1",
        "LSRequiresIPhoneOS": True,
        "NSHighResolutionCapable": True,
        "CFBundleIconFiles": ["AppIcon"],
        "UILaunchScreen": {},
        "UIRequiredDeviceCapabilities": ["arm64"],
        "UIRequiresFullScreen": True,
        "UISupportedInterfaceOrientations": [
            "UIInterfaceOrientationLandscapeLeft",
            "UIInterfaceOrientationLandscapeRight",
        ],
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        plistlib.dump(data, stream, sort_keys=True)


def create_app_icon(context: PackContext, path: pathlib.Path) -> None:
    system_assets = context.project_dir / "Assets" / "System"
    sources = (
        system_assets / "icon.icns",
        system_assets / "icon.png",
    )
    source = next((candidate for candidate in sources if candidate.is_file()), None)
    if source is None:
        raise PackError(
            f"Project icon was not found in {system_assets}.",
            EXIT_PROJECT,
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    sips = shutil.which("sips")
    if sips:
        result = run_capture(
            [sips, "-s", "format", "png", "-z", "180", "180", str(source), "--out", str(path)],
            timeout=30,
        )
        if result.returncode == 0 and path.is_file():
            return
    if source.suffix.lower() == ".png":
        shutil.copy2(source, path)
        return
    fallback = system_assets / "icon.png"
    if fallback.is_file():
        shutil.copy2(fallback, path)
        return
    raise PackError(f"Unable to create the iOS app icon from {source}.", EXIT_PROJECT)


def cached_dependency_arguments(project_dir: pathlib.Path) -> list[str]:
    dependency_names = (
        "flac",
        "freetype",
        "harfbuzz",
        "libssh2",
        "mbedtls",
        "ogg",
        "sheenbidi",
        "vorbis",
    )
    cache_roots = (
        project_dir / "build" / "ios" / "_deps",
        project_dir / "build" / "_deps",
        project_dir / "build" / "Release" / "_deps",
        project_dir / "build" / "Debug" / "_deps",
    )
    arguments: list[str] = []
    for dependency_name in dependency_names:
        for cache_root in cache_roots:
            source_dir = cache_root / f"{dependency_name}-src"
            if (source_dir / "CMakeLists.txt").is_file():
                variable = f"FETCHCONTENT_SOURCE_DIR_{dependency_name.upper()}"
                arguments.append(f"-D{variable}={source_dir}")
                break
    return arguments


def xcode_build_command(
    context: PackContext,
    xcode_project: pathlib.Path,
    derived_data: pathlib.Path,
    device: dict[str, object] | None,
) -> list[str]:
    destination = "generic/platform=iOS"
    if device is not None:
        destination = f"platform=iOS,id={device_identifier(device)}"
    command = [
        "xcodebuild",
        "-project",
        str(xcode_project),
        "-scheme",
        "Main",
        "-configuration",
        "Release",
        "-sdk",
        "iphoneos",
        "-destination",
        destination,
        "-derivedDataPath",
        str(derived_data),
        "-allowProvisioningUpdates",
    ]
    if device is not None:
        command.append("-allowProvisioningDeviceRegistration")
    command.extend(
        [
            "CODE_SIGN_STYLE=Automatic",
            f"DEVELOPMENT_TEAM={context.team_id}",
            f"PRODUCT_BUNDLE_IDENTIFIER={context.bundle_identifier}",
            "build",
        ]
    )
    return command


def configure_and_build(
    context: PackContext,
    device: dict[str, object] | None,
) -> pathlib.Path:
    build_dir = context.project_dir / "build" / "ios"
    generated_dir = build_dir / "generated"
    info_plist = generated_dir / "Info.plist"
    app_icon = generated_dir / "AppIcon.png"
    resources_dir = generated_dir / "Resources"
    scripts_dir = resources_dir / "Scripts"
    write_info_plist(context, info_plist)
    create_app_icon(context, app_icon)

    luasf_cmake = context.project_dir / "LuaSF" / "CMakeLists.txt"
    if not luasf_cmake.is_file():
        raise PackError(
            f"LuaSF dependency was not found: {luasf_cmake}. Run tools/init.sh first.",
            EXIT_PROJECT,
        )
    if resources_dir.exists():
        shutil.rmtree(resources_dir)
    for directory_name in ("Assets", "Data", "Scripts"):
        shutil.copytree(
            context.project_dir / directory_name,
            resources_dir / directory_name,
            ignore=shutil.ignore_patterns(".DS_Store", "*.anim.json"),
        )
    finalize_package(
        resources_dir,
        context.encrypt_shaders,
        context.encrypt_data,
    )
    if context.use_luac:
        compile_scripts(scripts_dir, resolve_luac())
    script_tools = pathlib.Path(
        os.environ.get("LUDORK_SCRIPT_TOOLS_EXECUTABLE", sys.argv[0])
    ).expanduser().resolve()
    if not script_tools.is_file():
        raise PackError(
            f"ScriptTools executable was not found: {script_tools}. Run tools/init.sh first.",
            EXIT_TOOLCHAIN,
        )

    configure_command = [
        str(context.cmake),
        "-S",
        str(context.project_dir),
        "-B",
        str(build_dir),
        "-G",
        "Xcode",
        "-DCMAKE_SYSTEM_NAME=iOS",
        "-DCMAKE_OSX_SYSROOT=iphoneos",
        "-DCMAKE_OSX_ARCHITECTURES=arm64",
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0",
        f"-DLUDORK_SCRIPT_TOOLS_EXECUTABLE={script_tools}",
        f"-DLUDORK_ASSETS_SOURCE_DIR={resources_dir / 'Assets'}",
        f"-DLUDORK_DATA_SOURCE_DIR={resources_dir / 'Data'}",
        f"-DLUDORK_SCRIPTS_SOURCE_DIR={scripts_dir}",
        "-DLUASF_BUILD_SHARED_SFML=OFF",
        "-DLUASF_GENERATE_LUA_STUB=OFF",
        f"-DLUDORK_IOS_APP_NAME={context.artifact_name}",
        f"-DLUDORK_IOS_BUNDLE_IDENTIFIER={context.bundle_identifier}",
        f"-DLUDORK_IOS_DEVELOPMENT_TEAM={context.team_id}",
        f"-DLUDORK_IOS_INFO_PLIST={info_plist}",
        f"-DLUDORK_IOS_ICON={app_icon}",
    ]
    configure_command.extend(cached_dependency_arguments(context.project_dir))
    run_streaming(
        configure_command,
        environment=context.environment,
        cwd=context.project_dir,
    )

    xcode_project = build_dir / "Main.xcodeproj"
    if not xcode_project.is_dir():
        raise PackError(f"Xcode project was not generated: {xcode_project}")
    derived_data = build_dir / "DerivedData"
    run_streaming(
        xcode_build_command(context, xcode_project, derived_data, device),
        environment=context.environment,
        cwd=build_dir,
    )

    expected = build_dir / "bin" / "Release" / f"{context.artifact_name}.app"
    if expected.is_dir():
        return expected
    candidates = sorted(
        (
            candidate
            for candidate in build_dir.rglob(f"{context.artifact_name}.app")
            if candidate.is_dir() and (candidate / "Info.plist").is_file()
        ),
        key=lambda candidate: candidate.stat().st_mtime,
        reverse=True,
    )
    if candidates:
        return candidates[0]
    raise PackError(
        f"Xcode build completed without producing {context.artifact_name}.app"
    )


def verify_app(context: PackContext, app_path: pathlib.Path) -> None:
    result = run_capture(
        ["codesign", "--verify", "--deep", "--strict", str(app_path)],
        environment=context.environment,
        timeout=60,
    )
    if result.returncode != 0:
        raise PackError("Code signature verification failed.\n" + result.stdout.strip())
    with (app_path / "Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleDisplayName") != context.game_name:
        raise PackError("The built app does not contain the configured game name.")
    if info.get("CFBundleIdentifier") != context.bundle_identifier:
        raise PackError("The built app does not contain the configured bundle identifier.")
    entry_name = "Entry.luac" if context.use_luac else "Entry.lua"
    for relative in (
        pathlib.Path("Assets"),
        pathlib.Path("Data"),
        pathlib.Path("Scripts") / entry_name,
    ):
        if not (app_path / relative).exists():
            raise PackError(f"The built app is missing runtime resource: {relative}")


def create_ipa(context: PackContext, app_path: pathlib.Path) -> pathlib.Path:
    context.dist_dir.mkdir(parents=True, exist_ok=True)
    ipa_path = context.dist_dir / f"{context.artifact_name}.ipa"
    temporary_ipa = context.dist_dir / f".{context.artifact_name}.ipa.tmp"
    if temporary_ipa.exists():
        temporary_ipa.unlink()
    stage_root = app_path.parent.parent / "ipa-stage"
    if stage_root.exists():
        shutil.rmtree(stage_root)
    payload = stage_root / "Payload"
    payload.mkdir(parents=True)
    shutil.copytree(app_path, payload / app_path.name, symlinks=True)
    try:
        run_streaming(
            [
                "/usr/bin/ditto",
                "-c",
                "-k",
                "--norsrc",
                "--keepParent",
                "Payload",
                str(temporary_ipa),
            ],
            environment=context.environment,
            cwd=stage_root,
        )
    finally:
        shutil.rmtree(stage_root, ignore_errors=True)
    if not temporary_ipa.is_file():
        raise PackError(f"IPA was not generated: {temporary_ipa}")
    with zipfile.ZipFile(temporary_ipa) as archive:
        expected_info = f"Payload/{app_path.name}/Info.plist"
        if expected_info not in archive.namelist():
            raise PackError(f"IPA is missing {expected_info}")
    os.replace(temporary_ipa, ipa_path)
    print(f"IPA: {ipa_path}", flush=True)
    return ipa_path


def install_and_launch(
    context: PackContext,
    app_path: pathlib.Path,
    device: dict[str, object],
) -> None:
    install_and_launch_on_device(
        device,
        app_path,
        context.bundle_identifier,
        context.game_name,
        environment=context.environment,
    )


def main(arguments: list[str] | None = None) -> int:
    arguments = parse_arguments(arguments)
    try:
        context = create_context(arguments)
        device: dict[str, object] | None = None
        if arguments.export_to_iphone:
            require_device_tools(context.environment)
            device = select_iphone(context.environment)
            identifier = device_identifier(device)
            print(
                f"iPhone: {str(device.get('name', 'iPhone')).strip()} ({identifier})",
                flush=True,
            )
        if arguments.check:
            print("iOS packaging prerequisites are ready.", flush=True)
            return 0
        app_path = configure_and_build(context, device)
        verify_app(context, app_path)
        create_ipa(context, app_path)
        if device is not None:
            install_and_launch(context, app_path, device)
            print("iOS packaging, installation, and launch completed.", flush=True)
        else:
            print("iOS packaging completed.", flush=True)
        return 0
    except PackError as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return exception.exit_code
    except KeyboardInterrupt:
        print("iOS packaging cancelled.", file=sys.stderr, flush=True)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
