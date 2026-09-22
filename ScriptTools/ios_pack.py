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
from dataclasses import dataclass

from .apple_signing import (
    IOS_DEVELOPMENT_TEAM_ENVIRONMENT,
    IOS_PROVISIONING_PROFILE_ENVIRONMENT,
    IOS_SIGNING_CERTIFICATE_ENVIRONMENT,
    IOS_SIGNING_IDENTITY_ENVIRONMENT,
    OptionSource,
    TemporaryKeychain,
    absolute_path,
    read_secret_lines,
    select_identity,
)
from .resource_constants import ANIMATION_CACHE_SUFFIX
from .pack_error import PackError
from .packaging_constants import (
    COMMON_DEPENDENCY_CACHE_DIRECTORIES,
    EXIT_PROJECT,
    EXIT_SIGNING,
    EXIT_TOOLCHAIN,
    MOBILE_DEPENDENCY_NAMES,
    MOBILE_PROJECT_DIRECTORIES,
)
from .packaging_names import artifact_name, read_app_name
from .packaging_metadata import PackageMetadata, add_package_arguments, read_package_metadata
from .resource_constants import RESOURCE_GROUPS
from .compile_lua import resolve_luac
from .ui_property_values import UiAssetError
from .ui_preview import prepare_registry
from .finalize_package import finalize_package
from .ldpak import (
    LdPakError,
    validate_ldpak_source,
    validate_runtime_ldpak_layout,
)
from .ios_device import device_identifier
from .ios_device import install_and_launch as install_and_launch_on_device
from .ios_device import require_device_tools
from .ios_device import requires_developer_trust
from .ios_device import select_iphone
from .ios_toolchain import ProvisioningProfile
from .ios_toolchain import choose_team_id
from .ios_toolchain import install_provisioning_profile
from .ios_toolchain import read_provisioning_profile
from .ios_toolchain import require_cmake
from .ios_toolchain import require_xcode_tools
from .ios_toolchain import resolve_cmake
from .ios_toolchain import resolve_developer_dir
from .ios_toolchain import run_capture
from .ios_toolchain import run_streaming
from .ios_toolchain import select_team_id
from .ios_toolchain import validate_provisioning_profile
from .ios_toolchain import xcode_account_team_ids


@dataclass(frozen=True)
class IOSSigningOptions:
    certificate: pathlib.Path
    certificate_password: str
    provisioning_profile: pathlib.Path
    signing_identity: str


class ManualSigningSession:
    def __init__(self, options: IOSSigningOptions) -> None:
        self.options = options
        self.identity = ""
        self.profile: ProvisioningProfile | None = None
        self._keychain: TemporaryKeychain | None = None
        self._installed_profile: pathlib.Path | None = None
        self._profile_existed = True

    def __enter__(self) -> ManualSigningSession:
        keychain = TemporaryKeychain()
        try:
            keychain.__enter__()
            keychain.add_certificate(
                self.options.certificate,
                self.options.certificate_password,
            )
            self.identity = select_identity(
                keychain.identities(),
                self.options.signing_identity,
            ).identifier
        except BaseException:
            keychain.close()
            raise
        self._keychain = keychain
        return self

    def __exit__(self, exception_type: object, exception: object, traceback: object) -> None:
        if self._keychain is not None:
            self._keychain.close()
            self._keychain = None
        if self._installed_profile is not None and not self._profile_existed:
            self._installed_profile.unlink(missing_ok=True)
            self._installed_profile = None

    def prepare(self, team_id: str, bundle_identifier: str) -> None:
        profile = read_provisioning_profile(self.options.provisioning_profile)
        validate_provisioning_profile(profile, team_id, bundle_identifier)
        self.profile = profile
        self._installed_profile, self._profile_existed = install_provisioning_profile(
            self.options.provisioning_profile,
            profile,
        )
        print(f"Provisioning profile: {profile.name}", flush=True)

    @property
    def keychain_path(self) -> pathlib.Path:
        if self._keychain is None:
            raise PackError("The iOS signing keychain is unavailable.", EXIT_SIGNING)
        return self._keychain.path

    @property
    def profile_name(self) -> str:
        if self.profile is None:
            raise PackError("The iOS provisioning profile was not prepared.", EXIT_SIGNING)
        return self.profile.name


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
        encrypt_saves: bool,
        use_ldpak: bool,
        metadata: PackageMetadata,
        signing: ManualSigningSession | None = None,
    ) -> None:
        self.metadata = metadata
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
        self.encrypt_saves = encrypt_saves
        self.use_ldpak = use_ldpak
        self.signing = signing

    @property
    def environment(self) -> dict[str, str]:
        environment = os.environ.copy()
        environment["DEVELOPER_DIR"] = str(self.developer_dir)
        return environment


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="pack_ios",
        usage="pack_ios [--check] [--version VERSION] [--dev | --release] [--compile-lua] [--encrypt-shaders] [--encrypt-data] [--encrypt-saves] [--use-ldpak] [--export-to-iphone] [--team-id TEAMID] [--certificate PATH.p12] [--provisioning-profile PATH.mobileprovision] [--signing-identity NAME] <project-folder> [dist-folder]",
    )
    add_package_arguments(parser)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--compile-lua", action="store_true")
    parser.add_argument("--encrypt-shaders", action="store_true")
    parser.add_argument("--encrypt-data", action="store_true")
    parser.add_argument("--encrypt-saves", action="store_true")
    parser.add_argument("--use-ldpak", action="store_true")
    parser.add_argument("--export-to-iphone", action="store_true")
    parser.add_argument("--team-id")
    parser.add_argument("--certificate", type=pathlib.Path)
    parser.add_argument("--provisioning-profile", type=pathlib.Path)
    parser.add_argument("--signing-identity")
    parser.add_argument(
        "--ignore-environment",
        action="store_true",
        help="Ignore the LUDORK_* signing environment variables and use only the command-line options",
    )
    parser.add_argument("project_folder")
    parser.add_argument("dist_folder", nargs="?")
    return parser.parse_args(arguments)


def resolve_signing_options(
    arguments: argparse.Namespace,
    source: OptionSource,
) -> IOSSigningOptions | None:
    certificate_text = source.value(
        IOS_SIGNING_CERTIFICATE_ENVIRONMENT,
        str(arguments.certificate) if arguments.certificate is not None else None,
    )
    profile_text = source.value(
        IOS_PROVISIONING_PROFILE_ENVIRONMENT,
        str(arguments.provisioning_profile)
        if arguments.provisioning_profile is not None
        else None,
    )
    if not certificate_text and not profile_text:
        return None
    if not certificate_text or not profile_text:
        raise PackError(
            "Manual iOS signing requires both a signing certificate and a provisioning profile.",
            EXIT_SIGNING,
        )
    certificate = absolute_path(
        certificate_text,
        "The iOS signing certificate",
        IOS_SIGNING_CERTIFICATE_ENVIRONMENT,
    )
    provisioning_profile = absolute_path(
        profile_text,
        "The iOS provisioning profile",
        IOS_PROVISIONING_PROFILE_ENVIRONMENT,
    )
    password = read_secret_lines(sys.stdin, 1, "iOS signing")[0]
    return IOSSigningOptions(
        certificate,
        password,
        provisioning_profile,
        source.value(IOS_SIGNING_IDENTITY_ENVIRONMENT, arguments.signing_identity),
    )



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
    for directory_name in MOBILE_PROJECT_DIRECTORIES:
        directory = project_dir / directory_name
        if not directory.is_dir():
            raise PackError(
                f"Required iOS project folder was not found: {directory}",
                EXIT_PROJECT,
            )
    read_app_name(project_dir)
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
        ffmpeg_configure = project_dir / "Engine" / "ThirdParty" / "ffmpeg" / "configure"
        ffmpeg_cmake = project_dir / "Engine" / "cmake" / "FFmpeg" / "CMakeLists.txt"
        if not ffmpeg_configure.is_file() or not ffmpeg_cmake.is_file():
            raise PackError(
                "The project enables FFmpeg but its iOS build sources are incomplete.",
                EXIT_PROJECT,
            )
    return project_dir


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


def create_context(
    arguments: argparse.Namespace,
    signing: ManualSigningSession | None,
    source: OptionSource,
) -> PackContext:
    if sys.platform != "darwin":
        raise PackError("iOS packaging is only supported on macOS.", EXIT_TOOLCHAIN)
    project_dir = resolve_project(arguments.project_folder)
    metadata = read_package_metadata(project_dir, arguments.version, arguments.dev)
    if arguments.use_ldpak:
        validate_ldpak_source(project_dir)
    dist_dir = (
        pathlib.Path(arguments.dist_folder).expanduser().resolve()
        if arguments.dist_folder
        else project_dir / "dist"
    )
    developer_dir = resolve_developer_dir()
    cmake = resolve_cmake()
    cmake_version = require_cmake(cmake)
    game_name = metadata.display_name
    tools = require_xcode_tools(developer_dir)
    team_id = select_team_id(
        source.value(IOS_DEVELOPMENT_TEAM_ENVIRONMENT, arguments.team_id),
        signing is not None,
    )
    if arguments.compile_lua:
        try:
            luac = resolve_luac()
        except RuntimeError as exception:
            raise PackError(str(exception), EXIT_TOOLCHAIN) from exception
        print(f"luac: {luac}")
    name = artifact_name(metadata.app_name)
    identifier = bundle_identifier(team_id, metadata.app_name)
    print(f"Xcode: {tools['xcodebuild'].splitlines()[0]}")
    print(f"CMake: {cmake_version}")
    print(f"Developer directory: {developer_dir}")
    print(f"Signing team: {team_id}")
    print(f"Code signing: {'manual' if signing is not None else 'automatic'}")
    print(f"Game name: {game_name}")
    print(f"Bundle identifier: {identifier}")
    if signing is not None:
        signing.prepare(team_id, identifier)
        print(f"Signing identity: {signing.identity}")
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
        arguments.encrypt_saves,
        arguments.use_ldpak,
        metadata,
        signing,
    )


def write_info_plist(context: PackContext, path: pathlib.Path) -> None:
    data = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": context.game_name,
        "CFBundleExecutable": "$(EXECUTABLE_NAME)",
        "CFBundleIdentifier": context.bundle_identifier,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": context.game_name,
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": context.metadata.version,
        "CFBundleVersion": context.metadata.apple_build_version,
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
    cache_roots = (
        project_dir / "build" / "ios" / "_deps",
        *(project_dir / relative for relative in COMMON_DEPENDENCY_CACHE_DIRECTORIES),
    )
    arguments: list[str] = []
    for dependency_name in MOBILE_DEPENDENCY_NAMES:
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
    ]
    if context.signing is None:
        command.append("-allowProvisioningUpdates")
        if device is not None:
            command.append("-allowProvisioningDeviceRegistration")
    command.extend(
        [
            f"DEVELOPMENT_TEAM={context.team_id}",
            f"PRODUCT_BUNDLE_IDENTIFIER={context.bundle_identifier}",
        ]
    )
    if context.signing is None:
        command.append("CODE_SIGN_STYLE=Automatic")
    else:
        command.extend(
            [
                "CODE_SIGN_STYLE=Manual",
                f"CODE_SIGN_IDENTITY={context.signing.identity}",
                f"PROVISIONING_PROFILE_SPECIFIER={context.signing.profile_name}",
            ]
        )
    command.append("build")
    return command


def copy_runtime_resources(
    context: PackContext,
    resources_dir: pathlib.Path,
) -> None:
    if context.use_ldpak:
        validate_ldpak_source(context.project_dir)
    if resources_dir.exists():
        shutil.rmtree(resources_dir)
    for directory_name in RESOURCE_GROUPS:
        shutil.copytree(
            context.project_dir / directory_name,
            resources_dir / directory_name,
            ignore=shutil.ignore_patterns(".DS_Store", "*" + ANIMATION_CACHE_SUFFIX),
        )


def configure_and_build(
    context: PackContext,
    device: dict[str, object] | None,
) -> pathlib.Path:
    build_dir = context.project_dir / "build" / "ios"
    generated_dir = build_dir / "generated"
    info_plist = generated_dir / "Info.plist"
    app_icon = generated_dir / "AppIcon.png"
    resources_dir = generated_dir / "Resources"
    write_info_plist(context, info_plist)
    create_app_icon(context, app_icon)

    luasf_cmake = context.project_dir / "Engine" / "ThirdParty" / "LuaSF" / "CMakeLists.txt"
    if not luasf_cmake.is_file():
        raise PackError(
            f"LuaSF dependency was not found: {luasf_cmake}. Run tools/init.sh first.",
            EXIT_PROJECT,
        )
    script_tools = pathlib.Path(
        os.environ.get("LUDORK_SCRIPT_TOOLS_EXECUTABLE", sys.argv[0])
    ).expanduser().resolve()
    ui_registry = prepare_registry(context.project_dir, script_tools)
    copy_runtime_resources(context, resources_dir)
    context.metadata.write_build_info(resources_dir)
    finalize_package(
        resources_dir,
        context.encrypt_shaders,
        context.encrypt_data,
        compile_lua_enabled=context.use_luac,
        use_ldpak=context.use_ldpak,
        registry=ui_registry,
    )
    resource_options: list[str] = []
    for name in RESOURCE_GROUPS:
        source_dir = "" if context.use_ldpak else str(resources_dir / name)
        package_file = (
            str(resources_dir / f"{name}.ldpak") if context.use_ldpak else ""
        )
        resource_options.extend((
            f"-DLUDORK_{name.upper()}_SOURCE_DIR={source_dir}",
            f"-DLUDORK_{name.upper()}_PACKAGE_FILE={package_file}",
        ))
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
        "-DLUDORK_BUILD_UI_PREVIEW_HOST=OFF",
        f"-DLUDORK_UI_REGISTRY_PATH={ui_registry}",
        *resource_options,
        "-DLUASF_BUILD_SHARED_SFML=OFF",
        "-DLUASF_GENERATE_LUA_STUB=OFF",
        f"-DLUDORK_SAVE_AS_LDC={'ON' if context.encrypt_saves else 'OFF'}",
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
    if (
        info.get("CFBundleName") != context.game_name
        or info.get("CFBundleShortVersionString") != context.metadata.version
        or info.get("CFBundleVersion") != context.metadata.apple_build_version
    ):
        raise PackError("The built app does not contain the configured name and version.")
    if info.get("CFBundleIdentifier") != context.bundle_identifier:
        raise PackError("The built app does not contain the configured bundle identifier.")
    try:
        validate_runtime_ldpak_layout(
            app_path,
            context.use_ldpak,
        )
    except LdPakError as exception:
        raise PackError(str(exception), EXIT_PROJECT) from exception


def create_ipa(context: PackContext, app_path: pathlib.Path) -> pathlib.Path:
    context.dist_dir.mkdir(parents=True, exist_ok=True)
    ipa_path = context.dist_dir / f"{context.metadata.package_name}.ipa"
    temporary_ipa = context.dist_dir / f".{context.metadata.package_name}.ipa.tmp"
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


def package(
    arguments: argparse.Namespace,
    signing: ManualSigningSession | None,
    source: OptionSource,
) -> int:
    context = create_context(arguments, signing, source)
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


def main(arguments: list[str] | None = None) -> int:
    arguments = parse_arguments(arguments)
    try:
        source = OptionSource(arguments.ignore_environment)
        signing = resolve_signing_options(arguments, source)
        if signing is None:
            return package(arguments, None, source)
        with ManualSigningSession(signing) as session:
            return package(arguments, session, source)
    except PackError as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return exception.exit_code
    except (LdPakError, UiAssetError) as exception:
        print(f"Error: {exception}", file=sys.stderr, flush=True)
        return EXIT_PROJECT
    except KeyboardInterrupt:
        print("iOS packaging cancelled.", file=sys.stderr, flush=True)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
