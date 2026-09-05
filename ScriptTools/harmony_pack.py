from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unicodedata
import zipfile
from dataclasses import dataclass

from ScriptTools.compile_lua import resolve_luac
from ScriptTools.finalize_package import finalize_package
from ScriptTools.ldpak import (
    LdPakError,
    validate_ldpak_source,
    validate_ldpak,
    validate_runtime_ldpak_layout,
)


EXIT_TOOLCHAIN = 20
EXIT_DEVICE = 21
EXIT_SIGNING = 22
EXIT_PROJECT = 23
EXIT_APP_NAME_UNCHANGED = 24
DEFAULT_APP_NAME_PATTERN = re.compile(
    r"^[ \t]*local[ \t]+APP_NAME[ \t]*=[ \t]*[\"']LudorkSample[\"'][ \t]*(?:--[^\r\n]*)?\r?$",
    re.MULTILINE,
)
DEVECO_APP = pathlib.Path("/Applications/DevEco-Studio.app")
RUNTIME_ARCHIVE_NAME = "ludork-runtime.zip"
FILE_BUFFER_SIZE = 1024 * 1024
HARMONY_SDK_VERSION = "6.0.2(22)"
HARMONY_COMPATIBLE_API = 22
HARMONY_COMPILER_TARGET = "aarch64-linux-ohos22.0.0"
HARMONY_MOBILE_DEVICE_TYPES = frozenset(("default", "phone", "tablet"))
HARMONY_SIGNING_CONTRACT_VERSION = 2
BUNDLE_NAME_PATTERN = re.compile(
    r"^[A-Za-z](?:[A-Za-z0-9_]*[A-Za-z0-9])?"
    r"(?:\.[A-Za-z0-9](?:[A-Za-z0-9_]*[A-Za-z0-9])?){2,}$"
)
TEMPLATE_TOKEN_PATTERN = re.compile(r"__LUDORK_[A-Z0-9_]+__")
SIGNING_MATERIAL_FIELDS = (
    "storeFile",
    "storePassword",
    "keyAlias",
    "keyPassword",
    "signAlg",
    "profile",
    "certpath",
)
SIGNING_PATH_FIELDS = ("storeFile", "profile", "certpath")
SIGNING_WAIT_SECONDS = 180.0
SIGNING_POLL_SECONDS = 0.5
SIGNING_VALIDATION_TIMEOUT_SECONDS = 5.0
SIGNING_INVALID_RECHECK_SECONDS = 2.0
SIGNING_FINAL_VALIDATION_TIMEOUT_SECONDS = 5.0
HARMONY_LAUNCH_FOREGROUND_TIMEOUT_SECONDS = 10.0
HARMONY_LAUNCH_POLL_SECONDS = 0.5
HARMONY_LAUNCH_QUERY_TIMEOUT_SECONDS = 3.0
HARMONY_LAUNCH_ATTEMPTS = 2
HARMONY_LAUNCH_DIAGNOSTIC_LIMIT = 2048
INVALID_SIGNING_FINGERPRINT_LIMIT = 64
NATIVE_BUILD_LOG_READ_LIMIT = 512 * 1024
NATIVE_BUILD_DIAGNOSTIC_LIMIT = 16 * 1024
NATIVE_BUILD_DIAGNOSTIC_LINE_LIMIT = 64
ANSI_ESCAPE_PATTERN = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
SENSITIVE_ASSIGNMENT_PATTERN = re.compile(
    r"(?i)([\"']?(?:storePassword|keyPassword|password|passwd|token|secret|"
    r"apiKey|clientSecret|udid|deviceId|target)[\"']?)"
    r"(\s*[:=]\s*)"
    r"(?:\"(?:\\.|[^\"\\\r\n])*\"|'(?:\\.|[^'\\\r\n])*'|[^\s,;}\]]+)"
)
SENSITIVE_CLI_PATTERN = re.compile(
    r"(?i)(--(?:password|token|store-?password|key-?password)(?:=|\s+)|"
    r"-(?:storepass|keypass)(?:=|\s+))"
    r"(?:\"(?:\\.|[^\"\\\r\n])*\"|'(?:\\.|[^'\\\r\n])*'|\S+)"
)
SENSITIVE_AUTHORIZATION_PATTERN = re.compile(
    r"(?i)([\"']?authorization[\"']?\s*[:=]\s*)[^\r\n]*"
)
SENSITIVE_AUTHORIZATION_CLI_PATTERN = re.compile(
    r"(?i)(--authorization(?:=|\s+))[^\r\n]*"
)
SIGNING_MATERIAL_PATH_PATTERN = re.compile(
    r"(?i)(?:\"[^\"\r\n]*\.(?:p12|p7b|cer|pem|jks|keystore)\"|"
    r"'[^'\r\n]*\.(?:p12|p7b|cer|pem|jks|keystore)'|"
    r"\S+\.(?:p12|p7b|cer|pem|jks|keystore))"
)
DEVICE_IDENTIFIER_PATTERN = re.compile(r"(?i)\b[0-9a-f]{64}\b")


class PackError(RuntimeError):
    def __init__(self, message: str, exit_code: int) -> None:
        super().__init__(message)
        self.exit_code = exit_code


@dataclass(frozen=True)
class DevEcoTools:
    app: pathlib.Path
    executable: pathlib.Path
    java_home: pathlib.Path
    node_home: pathlib.Path
    sdk_home: pathlib.Path
    hvigor: pathlib.Path
    hdc: pathlib.Path
    sign_tool: pathlib.Path
    json5_module: pathlib.Path
    readobj: pathlib.Path


@dataclass(frozen=True)
class HarmonyDevice:
    identifier: str
    udid: str | None = None
    device_type: str | None = None


@dataclass(frozen=True)
class SigningCandidate:
    overlay: dict[str, object]
    fingerprint: str


@dataclass(frozen=True)
class PackContext:
    project_dir: pathlib.Path
    dist_dir: pathlib.Path
    stage_dir: pathlib.Path
    signing_dir: pathlib.Path
    template_dir: pathlib.Path
    script_tools: pathlib.Path
    tools: DevEcoTools
    game_name: str
    artifact_name: str
    bundle_name: str
    device_form: str
    graphics_api: str
    use_luac: bool
    encrypt_shaders: bool
    encrypt_data: bool
    encrypt_saves: bool
    use_ldpak: bool


def resolve_deveco_tools() -> DevEcoTools:
    configured = os.environ.get("LUDORK_DEVECO_STUDIO", "").strip()
    if configured:
        app = pathlib.Path(configured).expanduser().resolve()
    else:
        candidates = (DEVECO_APP, pathlib.Path.home() / "Applications" / DEVECO_APP.name)
        app = next((candidate for candidate in candidates if candidate.is_dir()), DEVECO_APP)
    tools = DevEcoTools(
        app,
        app / "Contents" / "MacOS" / "devecostudio",
        app / "Contents" / "jbr" / "Contents" / "Home",
        app / "Contents" / "tools" / "node",
        app / "Contents" / "sdk",
        app / "Contents" / "tools" / "hvigor" / "bin" / "hvigorw",
        app / "Contents" / "sdk" / "default" / "openharmony" / "toolchains" / "hdc",
        app
        / "Contents"
        / "sdk"
        / "default"
        / "openharmony"
        / "toolchains"
        / "lib"
        / "hap-sign-tool.jar",
        app
        / "Contents"
        / "tools"
        / "hvigor"
        / "hvigor-ohos-plugin"
        / "node_modules"
        / "json5",
        app
        / "Contents"
        / "sdk"
        / "default"
        / "openharmony"
        / "native"
        / "llvm"
        / "bin"
        / "llvm-readobj",
    )
    required = (
        tools.app,
        tools.java_home / "bin" / "java",
        tools.node_home / "bin" / "node",
        tools.sdk_home / "default" / "openharmony" / "native" / "oh-uni-package.json",
        tools.hvigor,
        tools.json5_module / "package.json",
        tools.readobj,
    )
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise PackError(
            "DevEco Studio with the OpenHarmony native SDK was not found:\n"
            + "\n".join(missing),
            EXIT_TOOLCHAIN,
        )
    return tools


def require_device_export_tools(tools: DevEcoTools) -> None:
    required = (
        tools.executable,
        tools.hdc,
        tools.sign_tool,
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise PackError(
            "DevEco Studio device export tools were not found:\n" + "\n".join(missing),
            EXIT_TOOLCHAIN,
        )


def resolve_project(path: pathlib.Path) -> pathlib.Path:
    project_dir = path.expanduser().resolve()
    project_file = project_dir / "Main.proj"
    if not project_file.is_file():
        raise PackError(f"Main.proj was not found: {project_file}", EXIT_PROJECT)
    try:
        project_data = json.loads(project_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise PackError(f"Unable to read {project_file}: {exception}", EXIT_PROJECT) from exception
    if not isinstance(project_data, dict) or project_data.get("Cpp") is not True:
        raise PackError(
            "HarmonyOS packaging requires a C++ source project. Standalone projects are not supported.",
            EXIT_PROJECT,
        )
    required_directories = (
        "Assets",
        "Engine/Source",
        "Engine/Runtime",
        "Data",
        "include",
        "Engine/ThirdParty/LuaSF",
        "Engine/ThirdParty/lua-cjson",
        "Scripts",
        "Engine/Standard",
        "src",
        "Engine/ThirdParty/zlib",
    )
    for name in required_directories:
        directory = project_dir / name
        if not directory.is_dir():
            raise PackError(
                f"Required HarmonyOS project folder was not found: {directory}",
                EXIT_PROJECT,
            )
    for path_name in ("CMakeLists.txt", "Engine/PlatformHosts/Harmony/build-profile.json5"):
        path = project_dir / path_name
        if not path.is_file():
            raise PackError(f"Required HarmonyOS project file was not found: {path}", EXIT_PROJECT)
    entry_path = project_dir / "Scripts" / "Entry.lua"
    if not entry_path.is_file():
        raise PackError(f"Lua entry script was not found: {entry_path}", EXIT_PROJECT)
    try:
        entry_source = entry_path.read_text(encoding="utf-8")
    except OSError as exception:
        raise PackError(f"Unable to read {entry_path}: {exception}", EXIT_PROJECT) from exception
    if DEFAULT_APP_NAME_PATTERN.search(entry_source):
        raise PackError(
            "Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging.",
            EXIT_APP_NAME_UNCHANGED,
        )
    if project_data.get("ffmpeg") is True:
        for path in (
            project_dir / "Engine" / "ThirdParty" / "ffmpeg" / "configure",
            project_dir / "Engine" / "cmake" / "FFmpeg" / "build_ohos.sh",
        ):
            if not path.is_file():
                raise PackError(
                    "The project enables FFmpeg but its HarmonyOS build sources are incomplete: "
                    + str(path),
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
        raise PackError(f"Game title must be a non-empty string: {system_path}", EXIT_PROJECT)
    return title.strip()


def safe_artifact_name(game_name: str) -> str:
    normalized = unicodedata.normalize("NFC", game_name)
    safe = re.sub(r"[\x00-\x1f\x7f<>:\"/\\|?*;]+", "-", normalized)
    safe = re.sub(r"\s+", " ", safe).strip(" .")
    return (safe[:80].rstrip(" .") or "Ludork Game")


def harmony_bundle_name(game_name: str) -> str:
    ascii_name = (
        unicodedata.normalize("NFKD", game_name)
        .encode("ascii", "ignore")
        .decode("ascii")
        .lower()
    )
    slug = re.sub(r"[^a-z0-9]+", "", ascii_name)[:24] or "game"
    digest = hashlib.sha256(game_name.encode("utf-8")).hexdigest()[:10]
    return f"com.ludork.{slug}.{digest}"


def validate_bundle_name(bundle_name: str) -> None:
    if not 7 <= len(bundle_name) <= 128 or BUNDLE_NAME_PATTERN.fullmatch(bundle_name) is None:
        raise PackError(
            "Generated HarmonyOS bundle name is invalid: "
            f"{bundle_name}. It must contain 7 to 128 ASCII letters, digits, underscores, "
            "and at least three period-separated segments.",
            EXIT_PROJECT,
        )


def resolve_script_tools() -> pathlib.Path:
    configured = os.environ.get("LUDORK_SCRIPT_TOOLS_EXECUTABLE", "").strip()
    executable = pathlib.Path(configured or sys.argv[0]).expanduser().resolve()
    if not executable.is_file():
        raise PackError(
            f"ScriptTools executable was not found: {executable}",
            EXIT_TOOLCHAIN,
        )
    return executable


def create_context(arguments: argparse.Namespace) -> PackContext:
    if sys.platform != "darwin":
        raise PackError("HarmonyOS packaging is only supported on macOS.", EXIT_TOOLCHAIN)
    graphics_api = arguments.graphics_api
    if graphics_api is None:
        graphics_api = "opengl-es" if arguments.device_form == "mobile" else "opengl"
    if arguments.device_form == "mobile" and graphics_api != "opengl-es":
        raise PackError(
            "HarmonyOS mobile packaging supports only the OpenGL ES graphics backend.",
            EXIT_PROJECT,
        )
    project_dir = resolve_project(arguments.project_folder)
    if arguments.use_ldpak:
        validate_ldpak_source(project_dir)
    if arguments.compile_lua:
        try:
            resolve_luac()
        except RuntimeError as exception:
            raise PackError(str(exception), EXIT_TOOLCHAIN) from exception
    game_name = read_game_name(project_dir)
    dist_dir = (
        arguments.dist_folder.expanduser().resolve()
        if arguments.dist_folder is not None
        else project_dir / "dist"
    )
    bundle_name = harmony_bundle_name(game_name)
    validate_bundle_name(bundle_name)
    return PackContext(
        project_dir=project_dir,
        dist_dir=dist_dir,
        stage_dir=project_dir / "build" / "harmony" / "stage",
        signing_dir=(
            project_dir
            / "build"
            / "harmony"
            / "signing"
            / bundle_name
            / arguments.device_form
        ),
        template_dir=project_dir / "Engine" / "PlatformHosts" / "Harmony",
        script_tools=resolve_script_tools(),
        tools=resolve_deveco_tools(),
        game_name=game_name,
        artifact_name=safe_artifact_name(game_name),
        bundle_name=bundle_name,
        device_form=arguments.device_form,
        graphics_api=graphics_api,
        use_luac=arguments.compile_lua,
        encrypt_shaders=arguments.encrypt_shaders,
        encrypt_data=arguments.encrypt_data,
        encrypt_saves=arguments.encrypt_saves,
        use_ldpak=arguments.use_ldpak,
    )


def read_device_type(tools: DevEcoTools, identifier: str) -> str:
    try:
        result = subprocess.run(
            [
                str(tools.hdc),
                "-t",
                identifier,
                "shell",
                "param",
                "get",
                "const.product.devicetype",
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError(
            "Unable to determine the connected HarmonyOS device type.",
            EXIT_DEVICE,
        ) from exception
    device_type = result.stdout.strip().casefold()
    if result.returncode != 0 or not device_type or re.search(r"\s", device_type):
        raise PackError(
            "Unable to determine the connected HarmonyOS device type. "
            "Unlock the device and reconnect it.",
            EXIT_DEVICE,
        )
    return device_type


def connected_harmony_devices(tools: DevEcoTools) -> list[HarmonyDevice]:
    try:
        result = subprocess.run(
            [str(tools.hdc), "list", "targets", "-v"],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError(
            "Unable to query connected HarmonyOS devices.",
            EXIT_DEVICE,
        ) from exception
    if result.returncode != 0:
        raise PackError(
            "Unable to query connected HarmonyOS devices. Restart HDC and try again.",
            EXIT_DEVICE,
        )
    devices: list[HarmonyDevice] = []
    identifiers: set[str] = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) < 4 or fields[2] != "Connected":
            continue
        identifier = fields[0].strip()
        if identifier and identifier not in identifiers:
            identifiers.add(identifier)
            devices.append(
                HarmonyDevice(
                    identifier=identifier,
                    device_type=read_device_type(tools, identifier),
                )
            )
    return devices


def device_matches_form(device: HarmonyDevice, device_form: str) -> bool:
    if device.device_type is None:
        return False
    if device_form == "mobile":
        return device.device_type in HARMONY_MOBILE_DEVICE_TYPES
    return device.device_type == "2in1"


def select_harmony_device(tools: DevEcoTools, device_form: str) -> HarmonyDevice:
    devices = connected_harmony_devices(tools)
    matching = [device for device in devices if device_matches_form(device, device_form)]
    form_name = "mobile" if device_form == "mobile" else "2in1"
    if not matching:
        raise PackError(
            f"No connected HarmonyOS {form_name} device was found. Connect and unlock one, "
            "then enable Developer Mode.",
            EXIT_DEVICE,
        )
    if len(matching) > 1:
        raise PackError(
            f"More than one connected HarmonyOS {form_name} device was found. "
            "Leave exactly one matching device connected.",
            EXIT_DEVICE,
        )
    return matching[0]


def read_device_udid(tools: DevEcoTools, device: HarmonyDevice) -> HarmonyDevice:
    try:
        result = subprocess.run(
            [str(tools.hdc), "-t", device.identifier, "shell", "bm", "get", "-u"],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError("Unable to read the HarmonyOS device UDID.", EXIT_DEVICE) from exception
    if result.returncode != 0:
        raise PackError(
            "Unable to read the HarmonyOS device UDID. Unlock the device and reconnect it.",
            EXIT_DEVICE,
        )
    lines = result.stdout.splitlines()
    for index, line in enumerate(lines):
        if "udid of current device" not in line.casefold():
            continue
        inline_value = line.partition(":")[2].strip()
        if re.fullmatch(r"[0-9A-Fa-f]{64}", inline_value):
            return HarmonyDevice(device.identifier, inline_value, device.device_type)
        for value in lines[index + 1 :]:
            candidate = value.strip()
            if not candidate:
                continue
            if re.fullmatch(r"[0-9A-Fa-f]{64}", candidate):
                return HarmonyDevice(device.identifier, candidate, device.device_type)
            break
    raise PackError(
        "The connected HarmonyOS device returned an invalid UDID.",
        EXIT_DEVICE,
    )


def ensure_harmony_device_connected(context: PackContext, device: HarmonyDevice) -> None:
    selected = select_harmony_device(context.tools, context.device_form)
    if selected.identifier != device.identifier:
        raise PackError(
            "The selected HarmonyOS device changed during packaging. Reconnect the original device and retry.",
            EXIT_DEVICE,
        )
    if selected.device_type != device.device_type:
        raise PackError(
            "The selected HarmonyOS device type changed during packaging. Reconnect the original device and retry.",
            EXIT_DEVICE,
        )
    if (
        device.udid is not None
        and read_device_udid(context.tools, selected).udid != device.udid
    ):
        raise PackError(
            "The connected HarmonyOS device changed during packaging. Reconnect the original device and retry.",
            EXIT_DEVICE,
        )


def copy_runtime_resources(context: PackContext, destination: pathlib.Path) -> None:
    if context.use_ldpak:
        validate_ldpak_source(context.project_dir)
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name in ("Assets", "Data", "Scripts"):
        shutil.copytree(
            context.project_dir / name,
            destination / name,
            ignore=shutil.ignore_patterns(".DS_Store", "*.anim.json"),
        )
    licenses = context.project_dir / "Licenses"
    if licenses.is_dir():
        shutil.copytree(
            licenses,
            destination / "Licenses",
            ignore=shutil.ignore_patterns(".DS_Store"),
        )
    for name in ("LICENSE.md", "THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES_zh_CN.md"):
        source = context.project_dir / name
        if source.is_file():
            shutil.copy2(source, destination / name)
    finalize_package(
        destination,
        context.encrypt_shaders,
        context.encrypt_data,
        compile_lua_enabled=context.use_luac,
        use_ldpak=context.use_ldpak,
    )
    validate_runtime_ldpak_layout(destination, context.use_ldpak)


def write_deterministic_zip(source: pathlib.Path, destination: pathlib.Path) -> str:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    if temporary.exists():
        temporary.unlink()
    entries = sorted(source.rglob("*"), key=lambda path: path.relative_to(source).as_posix())
    with zipfile.ZipFile(
        temporary,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
        strict_timestamps=True,
    ) as archive:
        for path in entries:
            relative = path.relative_to(source).as_posix()
            if path.is_dir():
                info = zipfile.ZipInfo(relative.rstrip("/") + "/", (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (0o40755 << 16) | 0x10
                archive.writestr(info, b"")
                continue
            info = zipfile.ZipInfo(relative, (1980, 1, 1, 0, 0, 0))
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            compression = (
                zipfile.ZIP_STORED
                if path.suffix.casefold() == ".ldpak"
                else zipfile.ZIP_DEFLATED
            )
            info.compress_type = compression
            info._compresslevel = 9
            info.file_size = path.stat().st_size
            with path.open("rb") as source_stream:
                with archive.open(info, "w") as archive_stream:
                    shutil.copyfileobj(
                        source_stream,
                        archive_stream,
                        length=FILE_BUFFER_SIZE,
                    )
    with temporary.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    temporary.replace(destination)
    return digest


def _valid_runtime_archive_path(path: str) -> bool:
    if not path or path.startswith("/") or "\\" in path:
        return False
    parts = path.rstrip("/").split("/")
    return bool(parts) and all(part not in {"", ".", ".."} for part in parts)


def validate_runtime_zip(
    archive_path: pathlib.Path,
    expected_use_ldpak: bool,
) -> None:
    with zipfile.ZipFile(archive_path) as archive:
        infos = archive.infolist()
        names = [info.filename for info in infos]
        if len(names) != len(set(names)):
            raise PackError("The HarmonyOS runtime ZIP contains duplicate entries.", 1)
        invalid = [name for name in names if not _valid_runtime_archive_path(name)]
        if invalid:
            raise PackError(
                "The HarmonyOS runtime ZIP contains unsafe paths:\n"
                + "\n".join(invalid),
                1,
            )
        file_infos = [info for info in infos if not info.is_dir()]
        file_names = {info.filename for info in file_infos}
        forbidden = sorted(
            name
            for name in file_names
            if pathlib.PurePosixPath(name).name == ".DS_Store"
            or name.casefold().endswith(".d.lua")
            or name.endswith(".anim.json")
            or "/Scripts/stub/" in "/" + name
        )
        if forbidden:
            raise PackError(
                "The HarmonyOS runtime ZIP contains development-only files:\n"
                + "\n".join(forbidden),
                1,
            )
        has_loose_scripts = any(name.startswith("Scripts/") for name in names)
        has_packed_scripts = "Scripts.ldpak" in file_names
        if has_loose_scripts == has_packed_scripts:
            raise PackError(
                "The HarmonyOS runtime ZIP must contain exactly one of Scripts or Scripts.ldpak.",
                1,
            )
        if has_packed_scripts != expected_use_ldpak:
            raise PackError("The HarmonyOS runtime ZIP has the wrong Scripts layout.", 1)
        if has_loose_scripts and not (
            {"Scripts/Entry.lua", "Scripts/Entry.luac"} & file_names
        ):
            raise PackError(
                "The HarmonyOS runtime ZIP is missing a Lua entry script.",
                1,
            )
        for prefix in ("Assets/", "Data/"):
            resource_entries = {
                name
                for name in names
                if name.startswith(prefix) and name != prefix
            }
            resource_files = {
                name for name in file_names if name.startswith(prefix)
            }
            if not resource_files:
                raise PackError(
                    f"The HarmonyOS runtime ZIP contains no {prefix.rstrip('/')} files.",
                    1,
                )
            package_files = {
                name
                for name in resource_files
                if "/" not in name[len(prefix) :]
                and name.casefold().endswith(".ldpak")
                and len(name) > len(prefix) + len(".ldpak")
            }
            if expected_use_ldpak and (
                package_files != resource_files
                or resource_entries != resource_files
            ):
                raise PackError(
                    f"The HarmonyOS packed runtime ZIP contains loose {prefix.rstrip('/')} entries.",
                    1,
                )
            if not expected_use_ldpak and package_files:
                raise PackError(
                    f"The HarmonyOS loose runtime ZIP contains packed {prefix.rstrip('/')} groups.",
                    1,
                )
        compressed_ldpak = sorted(
            info.filename
            for info in file_infos
            if info.filename.casefold().endswith(".ldpak")
            and info.compress_type != zipfile.ZIP_STORED
        )
        if compressed_ldpak:
            raise PackError(
                "The HarmonyOS runtime ZIP compresses .ldpak files:\n"
                + "\n".join(compressed_ldpak),
                1,
            )
        with tempfile.TemporaryDirectory(prefix="ludork-harmony-runtime-") as temporary:
            temporary_root = pathlib.Path(temporary)
            for info in file_infos:
                is_group_package = (
                    info.filename == "Scripts.ldpak"
                    or any(
                        info.filename.startswith(prefix)
                        and "/" not in info.filename[len(prefix) :]
                        and info.filename.endswith(".ldpak")
                        for prefix in ("Assets/", "Data/")
                    )
                )
                if not is_group_package:
                    with archive.open(info) as stream:
                        while stream.read(FILE_BUFFER_SIZE):
                            pass
                    continue
                extracted = temporary_root / info.filename
                extracted.parent.mkdir(parents=True, exist_ok=True)
                with archive.open(info) as source, extracted.open("wb") as destination:
                    shutil.copyfileobj(source, destination, FILE_BUFFER_SIZE)
                group = extracted.name[: -len(".ldpak")]
                validate_ldpak(extracted, expected_group=group)


def json5_argument_text(values: list[str]) -> str:
    return json.dumps(" ".join(values), ensure_ascii=False)[1:-1]


def cmake_bracket_literal(value: str) -> str:
    delimiter = "="
    while f"]{delimiter}]" in value:
        delimiter += "="
    return f"[{delimiter}[{value}]{delimiter}]"


def cached_dependency_sources(project_dir: pathlib.Path) -> dict[str, pathlib.Path]:
    names = ("flac", "freetype", "harfbuzz", "libssh2", "mbedtls", "ogg", "sheenbidi", "vorbis")
    roots = (
        project_dir / "build" / "_deps",
        project_dir / "build" / "Release" / "_deps",
        project_dir / "build" / "Debug" / "_deps",
    )
    result: dict[str, pathlib.Path] = {}
    for name in names:
        for root in roots:
            source = root / f"{name}-src"
            if (source / "CMakeLists.txt").is_file():
                result[name.upper()] = source
                break
    return result


def cmake_configuration(context: PackContext) -> str:
    lines = [
        f"set(LUDORK_PROJECT_DIR {cmake_bracket_literal(str(context.project_dir))})",
        "set(LUDORK_SCRIPT_TOOLS_EXECUTABLE "
        f"{cmake_bracket_literal(str(context.script_tools))} CACHE FILEPATH \"\" FORCE)",
        "set(LUDORK_SAVE_AS_LDC "
        f"{'ON' if context.encrypt_saves else 'OFF'} CACHE BOOL \"\" FORCE)",
    ]
    for name, source in cached_dependency_sources(context.project_dir).items():
        lines.append(
            f"set(FETCHCONTENT_SOURCE_DIR_{name} "
            f"{cmake_bracket_literal(str(source))} CACHE PATH \"\" FORCE)"
        )
    return "\n".join(lines)


def cmake_device_form(context: PackContext) -> str:
    return "MOBILE" if context.device_form == "mobile" else "2IN1"


def cmake_opengl_es(context: PackContext) -> str:
    return "ON" if context.graphics_api == "opengl-es" else "OFF"


def harmony_artifact_variant(context: PackContext) -> str:
    if context.device_form == "mobile":
        return "mobile"
    return f"2in1-{context.graphics_api}"


def replace_template_tokens(
    context: PackContext,
    stage_dir: pathlib.Path,
    runtime_hash: str,
) -> None:
    arguments = generated_native_arguments(context)
    app_environments = ""
    ability_form_options = '"orientation": "landscape",'
    cursor_permission = ""
    device_types = ["phone", "tablet"]
    if context.device_form == "2in1":
        app_environments = (
            ',\n    "appEnvironments": [\n'
            "      {\n"
            '        "name": "NEED_OPENGL",\n'
            '        "value": "1"\n'
            "      }\n"
            "    ]"
        )
        ability_form_options = (
            '"supportWindowMode": [\n'
            '          "fullscreen",\n'
            '          "floating"\n'
            "        ],"
        )
        cursor_permission = (
            "{\n"
            '        "name": "ohos.permission.LOCK_WINDOW_CURSOR"\n'
            "      },"
        )
        device_types = ["2in1"]
    replacements = {
        "__LUDORK_CMAKE_CONFIG__": cmake_configuration(context),
        "__LUDORK_RUNTIME_HASH__": runtime_hash,
        "__LUDORK_GRAPHICS_API__": "OpenGL ES" if context.graphics_api == "opengl-es" else "OpenGL",
        "__LUDORK_BUNDLE_NAME__": context.bundle_name,
        "__LUDORK_GAME_NAME__": json.dumps(
            context.game_name,
            ensure_ascii=False,
        )[1:-1],
        "__LUDORK_CMAKE_ARGUMENTS__": json5_argument_text(arguments),
        "__LUDORK_COMPILER_TARGET__": HARMONY_COMPILER_TARGET,
        "__LUDORK_APP_ENVIRONMENTS__": app_environments,
        "__LUDORK_DEVICE_TYPES__": json.dumps(device_types),
        "__LUDORK_ABILITY_FORM_OPTIONS__": ability_form_options,
        "__LUDORK_CURSOR_PERMISSION__": cursor_permission,
    }
    text_suffixes = {".json", ".json5", ".ts", ".ets", ".txt", ".cmake"}
    for path in sorted(stage_dir.rglob("*")):
        if not path.is_file() or (path.suffix not in text_suffixes and path.name != "CMakeLists.txt"):
            continue
        text = path.read_text(encoding="utf-8")
        for token, value in replacements.items():
            text = text.replace(token, value)
        path.write_text(text, encoding="utf-8")


def create_app_icon(context: PackContext, stage_dir: pathlib.Path) -> None:
    destination = stage_dir / "AppScope" / "resources" / "base" / "media" / "app_icon.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    system_assets = context.project_dir / "Assets" / "System"
    png = system_assets / "icon.png"
    if png.is_file():
        shutil.copy2(png, destination)
        return
    icns = system_assets / "icon.icns"
    sips = shutil.which("sips")
    if icns.is_file() and sips is not None:
        result = subprocess.run(
            [sips, "-s", "format", "png", "-z", "512", "512", str(icns), "--out", str(destination)],
            check=False,
        )
        if result.returncode == 0 and destination.is_file():
            return
    raise PackError(f"Project icon was not found in {system_assets}.", EXIT_PROJECT)


def prepare_stage(context: PackContext, stage_dir: pathlib.Path | None = None) -> str:
    destination = stage_dir or context.stage_dir
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(context.template_dir, destination)
    resources = context.project_dir / "build" / "harmony" / "runtime"
    copy_runtime_resources(context, resources)
    rawfile = destination / "entry" / "src" / "main" / "resources" / "rawfile"
    runtime_archive = rawfile / RUNTIME_ARCHIVE_NAME
    runtime_hash = write_deterministic_zip(resources, runtime_archive)
    validate_runtime_zip(runtime_archive, context.use_ldpak)
    (rawfile / "ludork-runtime.sha256").write_text(runtime_hash + "\n", encoding="ascii")
    create_app_icon(context, destination)
    replace_template_tokens(context, destination, runtime_hash)
    return runtime_hash


def read_json5(
    tools: DevEcoTools,
    path: pathlib.Path,
    timeout: float = 10.0,
) -> object | None:
    if not path.is_file():
        return None
    script = (
        "const fs=require('fs');"
        "const JSON5=require(process.argv[1]);"
        "const value=JSON5.parse(fs.readFileSync(process.argv[2],'utf8'));"
        "process.stdout.write(JSON.stringify(value));"
    )
    try:
        result = subprocess.run(
            [
                str(tools.node_home / "bin" / "node"),
                "-e",
                script,
                str(tools.json5_module),
                str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode != 0:
        return None
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        return None


def require_json5_object(tools: DevEcoTools, path: pathlib.Path) -> dict[str, object]:
    value = read_json5(tools, path)
    if not isinstance(value, dict):
        raise PackError(f"Unable to read generated HarmonyOS project file: {path}", EXIT_PROJECT)
    return value


def valid_harmony_project_profile(profile: dict[str, object]) -> bool:
    app = profile.get("app")
    modules = profile.get("modules")
    if not isinstance(app, dict) or not isinstance(modules, list):
        return False
    signing_configs = app.get("signingConfigs")
    products = app.get("products")
    build_modes = app.get("buildModeSet")
    if (
        not isinstance(signing_configs, list)
        or not isinstance(products, list)
        or not isinstance(build_modes, list)
    ):
        return False
    default_products = [
        product
        for product in products
        if isinstance(product, dict) and product.get("name") == "default"
    ]
    release_modes = [
        mode
        for mode in build_modes
        if isinstance(mode, dict) and mode.get("name") == "release"
    ]
    entry_modules = [
        module
        for module in modules
        if isinstance(module, dict) and module.get("name") == "entry"
    ]
    if len(default_products) != 1 or len(release_modes) != 1 or len(entry_modules) != 1:
        return False
    if default_products[0].get("runtimeOS") != "HarmonyOS":
        return False
    source_path = entry_modules[0].get("srcPath")
    if not isinstance(source_path, str) or not source_path.strip():
        return False
    targets = entry_modules[0].get("targets")
    if not isinstance(targets, list):
        return False
    default_targets = [
        target
        for target in targets
        if isinstance(target, dict) and target.get("name") == "default"
    ]
    if len(default_targets) != 1:
        return False
    applied_products = default_targets[0].get("applyToProducts")
    return isinstance(applied_products, list) and "default" in applied_products


def harmony_project_sdk_contract(profile: dict[str, object]) -> bool:
    app = profile.get("app")
    if not isinstance(app, dict):
        return False
    products = app.get("products")
    if not isinstance(products, list):
        return False
    default_products = [
        product
        for product in products
        if isinstance(product, dict) and product.get("name") == "default"
    ]
    if len(default_products) != 1:
        return False
    product = default_products[0]
    return (
        product.get("targetSdkVersion") == HARMONY_SDK_VERSION
        and product.get("compatibleSdkVersion") == HARMONY_SDK_VERSION
    )


def generated_native_arguments(context: PackContext) -> list[str]:
    return [
        f"-DSFML_HARMONY_DEVICE_FORM={cmake_device_form(context)}",
        f"-DSFML_OPENGL_ES={cmake_opengl_es(context)}",
        f"-DOHOS_COMPATIBLE_SDK_VERSION={HARMONY_COMPATIBLE_API}",
        "-DSFML_USE_SYSTEM_DEPS=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
    ]


def valid_generated_module(context: PackContext, profile: dict[str, object]) -> bool:
    module = profile.get("module")
    if not isinstance(module, dict):
        return False
    expected_device_types = ["phone", "tablet"] if context.device_form == "mobile" else ["2in1"]
    if module.get("deviceTypes") != expected_device_types:
        return False
    abilities = module.get("abilities")
    if not isinstance(abilities, list):
        return False
    entry_abilities = [
        ability
        for ability in abilities
        if isinstance(ability, dict) and ability.get("name") == "EntryAbility"
    ]
    if len(entry_abilities) != 1:
        return False
    ability = entry_abilities[0]
    permissions = module.get("requestPermissions")
    if not isinstance(permissions, list):
        return False
    permission_names = {
        permission.get("name")
        for permission in permissions
        if isinstance(permission, dict)
    }
    has_cursor_permission = "ohos.permission.LOCK_WINDOW_CURSOR" in permission_names
    if context.device_form == "mobile":
        return (
            ability.get("orientation") == "landscape"
            and "supportWindowMode" not in ability
            and not has_cursor_permission
        )
    return (
        "orientation" not in ability
        and ability.get("supportWindowMode") == ["fullscreen", "floating"]
        and has_cursor_permission
    )


def valid_generated_app_scope(context: PackContext, app: dict[str, object]) -> bool:
    environments = app.get("appEnvironments")
    if context.device_form == "mobile":
        return environments is None
    return environments == [{"name": "NEED_OPENGL", "value": "1"}]


def valid_generated_native_profile(
    context: PackContext,
    profile: dict[str, object],
) -> bool:
    build_option = profile.get("buildOption")
    if not isinstance(build_option, dict):
        return False
    native_options = build_option.get("externalNativeOptions")
    if not isinstance(native_options, dict):
        return False
    arguments = native_options.get("arguments")
    return isinstance(arguments, str) and arguments.split() == generated_native_arguments(context)


def valid_signing_native_profile(
    context: PackContext,
    profile: dict[str, object],
) -> bool:
    build_option = profile.get("buildOption")
    if not isinstance(build_option, dict):
        return False
    native_options = build_option.get("externalNativeOptions")
    if not isinstance(native_options, dict):
        return False
    arguments = native_options.get("arguments")
    if not isinstance(arguments, str):
        return False
    actual = arguments.split()
    expected = generated_native_arguments(context)
    if context.device_form != "2in1":
        return actual == expected
    graphics_prefix = "-DSFML_OPENGL_ES="
    graphics_indexes = [
        index for index, argument in enumerate(actual) if argument.startswith(graphics_prefix)
    ]
    if len(graphics_indexes) != 1:
        return False
    graphics_index = graphics_indexes[0]
    if actual[graphics_index] not in {f"{graphics_prefix}ON", f"{graphics_prefix}OFF"}:
        return False
    actual[graphics_index] = f"{graphics_prefix}{cmake_opengl_es(context)}"
    return actual == expected


def write_private_json(path: pathlib.Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.",
        suffix=".tmp",
        dir=path.parent,
    )
    temporary = pathlib.Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise


def validate_project_contract(
    context: PackContext,
    project_dir: pathlib.Path,
    signing_workspace: bool,
) -> None:
    text_suffixes = {
        ".cmake",
        ".ets",
        ".json",
        ".json5",
        ".properties",
        ".ts",
        ".txt",
        ".xml",
        ".yaml",
        ".yml",
    }
    for path in sorted(project_dir.rglob("*")):
        relative_parts = path.relative_to(project_dir).parts
        if any(part in {".hvigor", ".idea", "build", "oh_modules"} for part in relative_parts):
            continue
        if not path.is_file() or (path.suffix not in text_suffixes and path.name != "CMakeLists.txt"):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        token = TEMPLATE_TOKEN_PATTERN.search(text)
        if token is not None:
            raise PackError(
                f"Generated HarmonyOS project still contains template token {token.group(0)}: {path}",
                EXIT_PROJECT,
            )
    app_path = project_dir / "AppScope" / "app.json5"
    app_profile = require_json5_object(context.tools, app_path)
    app = app_profile.get("app")
    if (
        not isinstance(app, dict)
        or app.get("bundleName") != context.bundle_name
        or not valid_generated_app_scope(context, app)
    ):
        raise PackError(
            f"Generated HarmonyOS project has an invalid app contract in {app_path}.",
            EXIT_PROJECT,
        )
    project_profile_path = project_dir / "build-profile.json5"
    project_profile = require_json5_object(context.tools, project_profile_path)
    if (
        not valid_harmony_project_profile(project_profile)
        or not harmony_project_sdk_contract(project_profile)
    ):
        raise PackError(
            f"Generated HarmonyOS project has an invalid build profile: {project_profile_path}",
            EXIT_PROJECT,
        )
    module_path = project_dir / "entry" / "src" / "main" / "module.json5"
    module_profile = require_json5_object(context.tools, module_path)
    if not valid_generated_module(context, module_profile):
        raise PackError(
            f"Generated HarmonyOS project has an invalid device form contract: {module_path}",
            EXIT_PROJECT,
        )
    native_profile_path = project_dir / "entry" / "build-profile.json5"
    native_profile = require_json5_object(context.tools, native_profile_path)
    valid_native_profile = (
        valid_signing_native_profile(context, native_profile)
        if signing_workspace
        else valid_generated_native_profile(context, native_profile)
    )
    if not valid_native_profile:
        raise PackError(
            f"Generated HarmonyOS project has an invalid native build contract: {native_profile_path}",
            EXIT_PROJECT,
        )
    validate_bundle_name(context.bundle_name)


def validate_generated_project(context: PackContext, project_dir: pathlib.Path) -> None:
    validate_project_contract(context, project_dir, False)


def validate_signing_workspace(context: PackContext, project_dir: pathlib.Path) -> None:
    validate_project_contract(context, project_dir, True)


def absolute_signing_path(project_dir: pathlib.Path, value: str) -> pathlib.Path:
    path = pathlib.Path(os.path.expandvars(value)).expanduser()
    if not path.is_absolute():
        path = project_dir / path
    return path.resolve()


def signing_overlay_from_profile(
    context: PackContext,
    profile: dict[str, object],
    project_dir: pathlib.Path,
) -> dict[str, object] | None:
    if (
        not valid_harmony_project_profile(profile)
        or not harmony_project_sdk_contract(profile)
    ):
        return None
    app = profile.get("app")
    if not isinstance(app, dict):
        return None
    products = app.get("products")
    signing_configs = app.get("signingConfigs")
    if not isinstance(products, list) or not isinstance(signing_configs, list):
        return None
    default_products = [
        product
        for product in products
        if isinstance(product, dict) and product.get("name") == "default"
    ]
    if len(default_products) != 1:
        return None
    signing_reference = default_products[0].get("signingConfig")
    if isinstance(signing_reference, str) and signing_reference.strip():
        signing_name = signing_reference.rsplit(".", 1)[-1]
    else:
        implicit_configs = [
            config
            for config in signing_configs
            if isinstance(config, dict) and config.get("name") == "default"
        ]
        if len(signing_configs) != 1 or len(implicit_configs) != 1:
            return None
        signing_reference = "default"
        signing_name = "default"
    matching_configs = [
        config
        for config in signing_configs
        if isinstance(config, dict) and config.get("name") == signing_name
    ]
    if len(matching_configs) != 1:
        return None
    config = matching_configs[0]
    material = config.get("material")
    if not isinstance(material, dict):
        return None
    normalized_material: dict[str, str] = {}
    for field in SIGNING_MATERIAL_FIELDS:
        value = material.get(field)
        if not isinstance(value, str) or not value.strip():
            return None
        normalized_material[field] = value
    for field in SIGNING_PATH_FIELDS:
        path = absolute_signing_path(project_dir, normalized_material[field])
        if not path.is_file():
            return None
        normalized_material[field] = str(path)
    config_type = config.get("type")
    if config_type != "HarmonyOS":
        return None
    normalized_config: dict[str, object] = {
        "name": signing_name,
        "material": normalized_material,
        "type": config_type,
    }
    return {
        "bundleName": context.bundle_name,
        "deviceForm": context.device_form,
        "compatibleSdkApi": HARMONY_COMPATIBLE_API,
        "contractVersion": HARMONY_SIGNING_CONTRACT_VERSION,
        "productSigningConfig": signing_reference,
        "signingConfig": normalized_config,
    }


def verify_profile(
    tools: DevEcoTools,
    profile_path: pathlib.Path,
    timeout: float = 20.0,
) -> dict[str, object] | None:
    with tempfile.TemporaryDirectory(prefix="ludork-harmony-profile-") as temporary:
        result_path = pathlib.Path(temporary) / "result.json"
        try:
            result = subprocess.run(
                [
                    str(tools.java_home / "bin" / "java"),
                    "-jar",
                    str(tools.sign_tool),
                    "verify-profile",
                    "-inFile",
                    str(profile_path),
                    "-outFile",
                    str(result_path),
                ],
                check=False,
                capture_output=True,
                timeout=timeout,
            )
        except (OSError, subprocess.TimeoutExpired):
            return None
        if result.returncode != 0 or not result_path.is_file():
            return None
        try:
            verification = json.loads(result_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return None
    if not isinstance(verification, dict) or verification.get("verifiedPassed") is not True:
        return None
    content = verification.get("content")
    if not isinstance(content, dict):
        return None
    return content


def profile_matches_export(
    content: dict[str, object],
    bundle_name: str,
    device_udid: str,
) -> bool:
    if content.get("type") != "debug":
        return False
    bundle_info = content.get("bundle-info")
    validity = content.get("validity")
    debug_info = content.get("debug-info")
    if not isinstance(bundle_info, dict) or bundle_info.get("bundle-name") != bundle_name:
        return False
    if not isinstance(validity, dict):
        return False
    not_before = validity.get("not-before")
    not_after = validity.get("not-after")
    if (
        isinstance(not_before, bool)
        or isinstance(not_after, bool)
        or not isinstance(not_before, (int, float))
        or not isinstance(not_after, (int, float))
    ):
        return False
    current_time = time.time()
    if current_time < not_before or current_time > not_after:
        return False
    if not isinstance(debug_info, dict) or debug_info.get("device-id-type") != "udid":
        return False
    device_ids = debug_info.get("device-ids")
    if not isinstance(device_ids, list):
        return False
    return any(isinstance(value, str) and value == device_udid for value in device_ids)


def signing_candidate_from_overlay(
    context: PackContext,
    overlay: dict[str, object],
    device: HarmonyDevice,
    validation_timeout: float = SIGNING_VALIDATION_TIMEOUT_SECONDS,
) -> SigningCandidate | None:
    if (
        device.udid is None
        or overlay.get("bundleName") != context.bundle_name
        or overlay.get("deviceForm") != context.device_form
        or overlay.get("compatibleSdkApi") != HARMONY_COMPATIBLE_API
        or overlay.get("contractVersion") != HARMONY_SIGNING_CONTRACT_VERSION
    ):
        return None
    config = overlay.get("signingConfig")
    reference = overlay.get("productSigningConfig")
    if not isinstance(config, dict) or not isinstance(reference, str):
        return None
    config_name = config.get("name")
    if (
        not isinstance(config_name, str)
        or not config_name
        or config.get("type") != "HarmonyOS"
        or reference.rsplit(".", 1)[-1] != config_name
    ):
        return None
    material = config.get("material")
    if not isinstance(material, dict):
        return None
    for field in SIGNING_MATERIAL_FIELDS:
        value = material.get(field)
        if not isinstance(value, str) or not value.strip():
            return None
    paths: dict[str, pathlib.Path] = {}
    for field in SIGNING_PATH_FIELDS:
        value = material[field]
        if not isinstance(value, str):
            return None
        path = pathlib.Path(value)
        try:
            if not path.is_absolute() or not path.is_file():
                return None
        except OSError:
            return None
        paths[field] = path
    content = verify_profile(
        context.tools,
        paths["profile"],
        timeout=validation_timeout,
    )
    if content is None or not profile_matches_export(content, context.bundle_name, device.udid):
        return None
    fingerprint_parts = [json.dumps(overlay, ensure_ascii=False, sort_keys=True)]
    try:
        for field in SIGNING_PATH_FIELDS:
            stat = paths[field].stat()
            fingerprint_parts.append(f"{field}:{stat.st_size}:{stat.st_mtime_ns}")
    except OSError:
        return None
    fingerprint = hashlib.sha256("\n".join(fingerprint_parts).encode("utf-8")).hexdigest()
    return SigningCandidate(overlay, fingerprint)


def signing_candidate_from_project(
    context: PackContext,
    project_dir: pathlib.Path,
    device: HarmonyDevice,
    validation_timeout: float = SIGNING_VALIDATION_TIMEOUT_SECONDS,
) -> SigningCandidate | None:
    started = time.monotonic()
    profile_path = project_dir / "build-profile.json5"
    profile = read_json5(
        context.tools,
        profile_path,
        timeout=validation_timeout,
    )
    if not isinstance(profile, dict):
        return None
    overlay = signing_overlay_from_profile(context, profile, project_dir)
    if overlay is None:
        return None
    remaining = validation_timeout - (time.monotonic() - started)
    if remaining <= 0:
        return None
    return signing_candidate_from_overlay(
        context,
        overlay,
        device,
        remaining,
    )


def signing_project_stamp(project_dir: pathlib.Path) -> tuple[int, int] | None:
    try:
        stat = (project_dir / "build-profile.json5").stat()
    except OSError:
        return None
    return stat.st_size, stat.st_mtime_ns


def load_signing_overlay(path: pathlib.Path) -> dict[str, object] | None:
    if not path.is_file():
        return None
    try:
        overlay = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    return overlay if isinstance(overlay, dict) else None


def inject_signing_overlay(
    context: PackContext,
    project_dir: pathlib.Path,
    overlay: dict[str, object],
) -> None:
    profile_path = project_dir / "build-profile.json5"
    profile = require_json5_object(context.tools, profile_path)
    app = profile.get("app")
    config = overlay.get("signingConfig")
    reference = overlay.get("productSigningConfig")
    if not isinstance(app, dict) or not isinstance(config, dict) or not isinstance(reference, str):
        raise PackError("Stored HarmonyOS signing configuration is invalid.", EXIT_SIGNING)
    if (
        overlay.get("bundleName") != context.bundle_name
        or overlay.get("deviceForm") != context.device_form
        or overlay.get("compatibleSdkApi") != HARMONY_COMPATIBLE_API
        or overlay.get("contractVersion") != HARMONY_SIGNING_CONTRACT_VERSION
    ):
        raise PackError(
            "Stored HarmonyOS signing configuration does not match the generated project contract.",
            EXIT_SIGNING,
        )
    products = app.get("products")
    if not isinstance(products, list):
        raise PackError("Generated HarmonyOS products configuration is invalid.", EXIT_PROJECT)
    default_products = [
        product
        for product in products
        if isinstance(product, dict) and product.get("name") == "default"
    ]
    if len(default_products) != 1:
        raise PackError("Generated HarmonyOS default product is invalid.", EXIT_PROJECT)
    app["signingConfigs"] = [config]
    default_products[0]["signingConfig"] = reference
    write_private_json(profile_path, profile)


def signing_overlay_path(context: PackContext) -> pathlib.Path:
    return context.signing_dir / "overlay.json"


def signing_overlay_fingerprint(overlay: dict[str, object]) -> str | None:
    config = overlay.get("signingConfig")
    if not isinstance(config, dict):
        return None
    material = config.get("material")
    if not isinstance(material, dict):
        return None
    fingerprint_parts = [json.dumps(overlay, ensure_ascii=False, sort_keys=True)]
    try:
        for field in SIGNING_PATH_FIELDS:
            value = material.get(field)
            if not isinstance(value, str):
                return None
            path = pathlib.Path(value)
            if not path.is_absolute() or not path.is_file():
                return None
            stat = path.stat()
            fingerprint_parts.append(f"{field}:{stat.st_size}:{stat.st_mtime_ns}")
    except OSError:
        return None
    return hashlib.sha256("\n".join(fingerprint_parts).encode("utf-8")).hexdigest()


def invalid_signing_fingerprints_path(context: PackContext) -> pathlib.Path:
    return context.signing_dir / "invalid-fingerprints.json"


def load_invalid_signing_fingerprints(context: PackContext) -> list[str]:
    path = invalid_signing_fingerprints_path(context)
    if not path.is_file():
        return []
    try:
        values = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    if not isinstance(values, list):
        return []
    return [
        value
        for value in values
        if isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value)
    ]


def invalidate_signing_overlay(
    context: PackContext,
    overlay: dict[str, object],
) -> None:
    overlay_path = signing_overlay_path(context)
    fingerprint = signing_overlay_fingerprint(overlay)
    if fingerprint is not None:
        invalid_fingerprints = load_invalid_signing_fingerprints(context)
        invalid_fingerprints = [
            value for value in invalid_fingerprints if value != fingerprint
        ]
        invalid_fingerprints.append(fingerprint)
        write_private_json(
            invalid_signing_fingerprints_path(context),
            invalid_fingerprints[-INVALID_SIGNING_FINGERPRINT_LIMIT:],
        )
    invalid_path = overlay_path.with_name(
        f"overlay.invalid-{time.time_ns()}-{os.getpid()}.json"
    )
    try:
        overlay_path.replace(invalid_path)
    except FileNotFoundError:
        return
    except OSError as exception:
        raise PackError(
            "Unable to invalidate the unusable HarmonyOS signing configuration.",
            EXIT_SIGNING,
        ) from exception


def existing_signing_workspaces(context: PackContext) -> list[pathlib.Path]:
    candidates = [context.signing_dir / "project"]
    attempts_dir = context.signing_dir / "attempts"
    if attempts_dir.is_dir():
        try:
            candidates.extend(path for path in attempts_dir.iterdir() if path.is_dir())
        except OSError:
            pass
    dated: list[tuple[int, pathlib.Path]] = []
    for workspace in candidates:
        profile_path = workspace / "build-profile.json5"
        try:
            if profile_path.is_file():
                dated.append((profile_path.stat().st_mtime_ns, workspace))
        except OSError:
            continue
    return [
        workspace
        for _, workspace in sorted(dated, key=lambda value: value[0], reverse=True)
    ]


def prepare_signing_workspace(context: PackContext) -> pathlib.Path:
    attempts_dir = context.signing_dir / "attempts"
    attempts_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    context.signing_dir.chmod(0o700)
    attempts_dir.chmod(0o700)
    workspace = pathlib.Path(tempfile.mkdtemp(prefix="project-", dir=attempts_dir))
    try:
        prepare_stage(context, workspace)
        validate_generated_project(context, workspace)
    except BaseException:
        shutil.rmtree(workspace, ignore_errors=True)
        raise
    return workspace


def reusable_signing_workspace(
    context: PackContext,
    workspaces: list[pathlib.Path],
    invalid_fingerprints: set[str],
) -> pathlib.Path | None:
    for workspace in workspaces:
        try:
            validate_signing_workspace(context, workspace)
        except (OSError, PackError):
            continue
        profile = read_json5(context.tools, workspace / "build-profile.json5")
        if not isinstance(profile, dict):
            continue
        overlay = signing_overlay_from_profile(context, profile, workspace)
        if overlay is not None:
            fingerprint = signing_overlay_fingerprint(overlay)
            if fingerprint is not None and fingerprint in invalid_fingerprints:
                continue
        return workspace
    return None


def deveco_process_ids(tools: DevEcoTools) -> set[int]:
    try:
        process = subprocess.run(
            ["/usr/bin/pgrep", "-x", tools.executable.name],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError(
            "Unable to monitor the DevEco Studio process.",
            EXIT_SIGNING,
        ) from exception
    if process.returncode == 1:
        return set()
    if process.returncode != 0:
        raise PackError(
            "Unable to monitor the DevEco Studio process.",
            EXIT_SIGNING,
        )
    return {
        int(value)
        for value in process.stdout.splitlines()
        if value.strip().isdigit()
    }


def launch_deveco_signing_project(
    context: PackContext,
    workspace: pathlib.Path,
) -> None:
    try:
        opened = subprocess.run(
            ["/usr/bin/open", "-a", str(context.tools.app), str(workspace)],
            check=False,
            capture_output=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError("Unable to open the signing project in DevEco Studio.", EXIT_SIGNING) from exception
    if opened.returncode != 0:
        raise PackError("Unable to open the signing project in DevEco Studio.", EXIT_SIGNING)
    process_deadline = time.monotonic() + 20.0
    while True:
        if deveco_process_ids(context.tools):
            return
        if time.monotonic() >= process_deadline:
            raise PackError("DevEco Studio did not finish starting.", EXIT_SIGNING)
        time.sleep(0.25)


def confirm_signing_candidate(
    context: PackContext,
    workspace: pathlib.Path,
    device: HarmonyDevice,
    validation_timeout: float = SIGNING_VALIDATION_TIMEOUT_SECONDS,
) -> SigningCandidate | None:
    first = signing_candidate_from_project(
        context,
        workspace,
        device,
        validation_timeout,
    )
    if first is None:
        return None
    time.sleep(SIGNING_POLL_SECONDS)
    second = signing_candidate_from_project(
        context,
        workspace,
        device,
        validation_timeout,
    )
    if second is None or second.fingerprint != first.fingerprint:
        return None
    return second


def wait_for_deveco_signing(
    context: PackContext,
    workspace: pathlib.Path,
    device: HarmonyDevice,
    invalid_fingerprints: set[str],
) -> SigningCandidate:
    print("HarmonyOS device export requires a valid automatic signing configuration.", flush=True)
    print(
        "In DevEco Studio, open File > Project Structure > Project > Signing Configs for the "
        "default product, enable automatic signing, sign in, and complete the device confirmation "
        "within 3 minutes. Remove an existing invalid signing config before applying automatic "
        "signing again.",
        flush=True,
    )
    print(f"Signing project: {workspace}", flush=True)
    launch_deveco_signing_project(context, workspace)
    deadline = time.monotonic() + SIGNING_WAIT_SECONDS
    stable_fingerprint: str | None = None
    stable_count = 0
    last_invalid_stamp: tuple[int, int] | None = None
    next_invalid_validation = 0.0
    while True:
        current_time = time.monotonic()
        remaining = deadline - current_time
        if remaining <= 0:
            final_candidate = confirm_signing_candidate(
                context,
                workspace,
                device,
                SIGNING_FINAL_VALIDATION_TIMEOUT_SECONDS,
            )
            if (
                final_candidate is not None
                and final_candidate.fingerprint not in invalid_fingerprints
            ):
                return final_candidate
            raise PackError(
                "HarmonyOS signing was not completed within 3 minutes.",
                EXIT_SIGNING,
            )
        profile_stamp = signing_project_stamp(workspace)
        should_validate = (
            stable_fingerprint is not None
            or profile_stamp != last_invalid_stamp
            or current_time >= next_invalid_validation
        )
        candidate: SigningCandidate | None = None
        if should_validate:
            candidate = signing_candidate_from_project(
                context,
                workspace,
                device,
                min(SIGNING_VALIDATION_TIMEOUT_SECONDS, remaining),
            )
            if (
                candidate is not None
                and candidate.fingerprint in invalid_fingerprints
            ):
                candidate = None
            if candidate is None:
                last_invalid_stamp = profile_stamp
                next_invalid_validation = (
                    time.monotonic() + SIGNING_INVALID_RECHECK_SECONDS
                )
                stable_fingerprint = None
                stable_count = 0
            elif candidate.fingerprint == stable_fingerprint:
                stable_count += 1
            else:
                stable_fingerprint = candidate.fingerprint
                stable_count = 1
            if candidate is not None and stable_count >= 2:
                return candidate
        application_closed = not deveco_process_ids(context.tools)
        if application_closed:
            final_candidate = confirm_signing_candidate(
                context,
                workspace,
                device,
                SIGNING_FINAL_VALIDATION_TIMEOUT_SECONDS,
            )
            if (
                final_candidate is not None
                and final_candidate.fingerprint not in invalid_fingerprints
            ):
                return final_candidate
            raise PackError(
                "DevEco Studio exited before signing was completed.",
                EXIT_SIGNING,
            )
        time.sleep(SIGNING_POLL_SECONDS)


def resolve_signing_overlay(
    context: PackContext,
    device: HarmonyDevice,
) -> dict[str, object]:
    overlay_path = signing_overlay_path(context)
    invalid_fingerprints = set(load_invalid_signing_fingerprints(context))
    workspaces = existing_signing_workspaces(context)
    overlay = load_signing_overlay(overlay_path)
    if overlay is not None:
        candidate = signing_candidate_from_overlay(context, overlay, device)
        if candidate is not None and candidate.fingerprint not in invalid_fingerprints:
            return candidate.overlay
    for workspace in workspaces:
        try:
            validate_signing_workspace(context, workspace)
        except (OSError, PackError):
            continue
        candidate = confirm_signing_candidate(context, workspace, device)
        if candidate is not None and candidate.fingerprint not in invalid_fingerprints:
            write_private_json(overlay_path, candidate.overlay)
            return candidate.overlay
    workspace = reusable_signing_workspace(
        context,
        workspaces,
        invalid_fingerprints,
    ) or prepare_signing_workspace(context)
    candidate = wait_for_deveco_signing(
        context,
        workspace,
        device,
        invalid_fingerprints,
    )
    write_private_json(overlay_path, candidate.overlay)
    return candidate.overlay


def verify_signed_hap(
    context: PackContext,
    hap: pathlib.Path,
    device: HarmonyDevice,
) -> None:
    if device.udid is None:
        raise PackError("The HarmonyOS device UDID is unavailable.", EXIT_DEVICE)
    with tempfile.TemporaryDirectory(prefix="ludork-harmony-hap-") as temporary:
        temporary_dir = pathlib.Path(temporary)
        certificate_path = temporary_dir / "certificate.cer"
        profile_path = temporary_dir / "profile.p7b"
        try:
            result = subprocess.run(
                [
                    str(context.tools.java_home / "bin" / "java"),
                    "-jar",
                    str(context.tools.sign_tool),
                    "verify-app",
                    "-inFile",
                    str(hap),
                    "-outCertChain",
                    str(certificate_path),
                    "-outProfile",
                    str(profile_path),
                ],
                check=False,
                capture_output=True,
                timeout=30,
            )
        except (OSError, subprocess.TimeoutExpired) as exception:
            raise PackError("Unable to verify the signed HarmonyOS HAP.", EXIT_SIGNING) from exception
        if (
            result.returncode != 0
            or not certificate_path.is_file()
            or not profile_path.is_file()
        ):
            raise PackError("The generated HarmonyOS HAP has an invalid signature.", EXIT_SIGNING)
        content = verify_profile(context.tools, profile_path)
        if content is None or not profile_matches_export(
            content,
            context.bundle_name,
            device.udid,
        ):
            raise PackError(
                "The generated HarmonyOS HAP signature does not match the app or connected device.",
                EXIT_SIGNING,
            )


def redact_native_build_diagnostic(text: str) -> str:
    redacted = SENSITIVE_AUTHORIZATION_PATTERN.sub(r"\1<redacted>", text)
    redacted = SENSITIVE_AUTHORIZATION_CLI_PATTERN.sub(r"\1<redacted>", redacted)
    redacted = SENSITIVE_ASSIGNMENT_PATTERN.sub(r"\1\2<redacted>", redacted)
    redacted = SENSITIVE_CLI_PATTERN.sub(r"\1<redacted>", redacted)
    redacted = SIGNING_MATERIAL_PATH_PATTERN.sub("<signing-material>", redacted)
    return DEVICE_IDENTIFIER_PATTERN.sub("<device-id>", redacted)


def native_build_log_states(
    stage_dir: pathlib.Path,
) -> dict[pathlib.Path, tuple[int, int]]:
    try:
        stage_root = stage_dir.resolve(strict=True)
        paths = list((stage_root / "entry" / ".cxx").glob("**/output.log"))
    except OSError:
        return {}
    states: dict[pathlib.Path, tuple[int, int]] = {}
    for path in paths:
        try:
            resolved = path.resolve(strict=True)
            resolved.relative_to(stage_root)
            stat = resolved.stat()
        except (OSError, ValueError):
            continue
        states[resolved] = (stat.st_mtime_ns, stat.st_size)
    return states


def read_native_build_diagnostic(
    stage_dir: pathlib.Path,
    baseline: dict[pathlib.Path, tuple[int, int]],
) -> tuple[pathlib.Path, str] | None:
    current_states = native_build_log_states(stage_dir)
    changed_logs = [
        (state[0], log)
        for log, state in current_states.items()
        if baseline.get(log) != state
    ]
    newest_log: pathlib.Path | None = None
    for _, log in sorted(changed_logs, key=lambda item: item[0], reverse=True):
        if newest_log is None:
            newest_log = log
        try:
            with log.open("rb") as stream:
                stream.seek(0, os.SEEK_END)
                size = stream.tell()
                stream.seek(max(0, size - NATIVE_BUILD_LOG_READ_LIMIT))
                text = stream.read().decode("utf-8", errors="replace")
        except OSError:
            continue
        text = (
            ANSI_ESCAPE_PATTERN.sub("", text)
            .replace("\r\n", "\n")
            .replace("\r", "\n")
        )
        lines = text.splitlines()
        failures = [
            index
            for index, line in enumerate(lines)
            if line.startswith("FAILED: ")
        ]
        if not failures:
            continue
        diagnostic_lines = [lines[failures[-1]]]
        command_skipped = False
        truncated = False
        for line in lines[failures[-1] + 1 :]:
            if not command_skipped:
                if not line.strip():
                    continue
                command_skipped = True
                continue
            if (
                line.startswith("FAILED: ")
                or line.startswith("ninja: build stopped:")
                or re.match(r"^\[\d+/\d+\]\s", line) is not None
            ):
                break
            diagnostic_lines.append(line)
            if len(diagnostic_lines) >= NATIVE_BUILD_DIAGNOSTIC_LINE_LIMIT:
                truncated = True
                break
        diagnostic = redact_native_build_diagnostic("\n".join(diagnostic_lines).strip())
        if len(diagnostic) > NATIVE_BUILD_DIAGNOSTIC_LIMIT:
            head_limit = NATIVE_BUILD_DIAGNOSTIC_LIMIT * 3 // 4
            tail_limit = NATIVE_BUILD_DIAGNOSTIC_LIMIT - head_limit
            diagnostic = (
                diagnostic[:head_limit].rstrip()
                + "\n... native build diagnostics truncated ...\n"
                + diagnostic[-tail_limit:].lstrip()
            )
        elif truncated:
            diagnostic += "\n... native build diagnostics truncated ..."
        return log, diagnostic
    return (newest_log, "") if newest_log is not None else None


def print_native_build_diagnostic(
    stage_dir: pathlib.Path,
    baseline: dict[pathlib.Path, tuple[int, int]],
) -> None:
    result = read_native_build_diagnostic(stage_dir, baseline)
    if result is None:
        return
    log, diagnostic = result
    if diagnostic:
        print("Condensed HarmonyOS native build failure:", file=sys.stderr, flush=True)
        print(diagnostic, file=sys.stderr, flush=True)
    print(f"Full native build log: {log}", file=sys.stderr, flush=True)


def validate_native_build_contract(context: PackContext) -> None:
    native_root = context.stage_dir / "entry" / ".cxx"
    compile_databases = sorted(native_root.glob("**/compile_commands.json"))
    if len(compile_databases) != 1:
        raise PackError(
            "The HarmonyOS native build did not produce one fresh CMake configuration.",
            EXIT_PROJECT,
        )
    cache_path = compile_databases[0].parent / "CMakeCache.txt"
    if not cache_path.is_file():
        raise PackError(
            "The HarmonyOS native build did not produce one fresh CMake configuration.",
            EXIT_PROJECT,
        )
    try:
        commands = json.loads(compile_databases[0].read_text(encoding="utf-8"))
        cache = cache_path.read_text(encoding="utf-8")
    except (OSError, json.JSONDecodeError) as exception:
        raise PackError(
            "Unable to validate the HarmonyOS native build configuration.",
            EXIT_PROJECT,
        ) from exception
    if not isinstance(commands, list) or not commands:
        raise PackError(
            "The HarmonyOS native compile database is empty.",
            EXIT_PROJECT,
        )
    target_argument = f"--target={HARMONY_COMPILER_TARGET}"
    for entry in commands:
        if not isinstance(entry, dict):
            raise PackError(
                "The HarmonyOS native compile database is invalid.",
                EXIT_PROJECT,
            )
        command = entry.get("command")
        arguments = entry.get("arguments")
        if isinstance(command, str):
            command_text = command
        elif isinstance(arguments, list):
            command_text = " ".join(
                value for value in arguments if isinstance(value, str)
            )
        else:
            command_text = ""
        if (
            target_argument not in command_text
            or "-D__MUSL__" not in command_text
            or "-Wunguarded-availability" not in command_text
        ):
            raise PackError(
                "A HarmonyOS native compilation command does not use the API 22 contract.",
                EXIT_PROJECT,
            )
    expected_cache_values = (
        f"SFML_HARMONY_DEVICE_FORM:STRING={cmake_device_form(context)}",
        f"SFML_OPENGL_ES:BOOL={cmake_opengl_es(context)}",
    )
    if any(value not in cache for value in expected_cache_values):
        raise PackError(
            "The HarmonyOS native CMake cache does not match the selected export variant.",
            EXIT_PROJECT,
        )
    native_sdk = (
        context.tools.sdk_home / "default" / "openharmony" / "native"
    )
    try:
        macros = subprocess.run(
            [
                str(native_sdk / "llvm" / "bin" / "clang"),
                target_argument,
                f"--sysroot={native_sdk / 'sysroot'}",
                "-dM",
                "-E",
                "-x",
                "c",
                "/dev/null",
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError(
            "Unable to validate the HarmonyOS API 22 compiler macros.",
            EXIT_TOOLCHAIN,
        ) from exception
    if macros.returncode != 0 or "#define __OHOS_Major__ 22" not in macros.stdout:
        raise PackError(
            "The HarmonyOS compiler target does not define __OHOS_Major__ as 22.",
            EXIT_TOOLCHAIN,
        )


def hap_native_dependencies(
    context: PackContext,
    hap: pathlib.Path,
) -> set[str]:
    with tempfile.TemporaryDirectory(prefix="ludork-harmony-native-") as temporary:
        library = pathlib.Path(temporary) / "libentry.so"
        try:
            with zipfile.ZipFile(hap) as archive:
                with archive.open("libs/arm64-v8a/libentry.so") as source:
                    with library.open("wb") as destination:
                        shutil.copyfileobj(source, destination)
            result = subprocess.run(
                [str(context.tools.readobj), "--needed-libs", str(library)],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
        except (OSError, KeyError, subprocess.TimeoutExpired) as exception:
            raise PackError(
                "Unable to inspect the HarmonyOS native library dependencies.",
                1,
            ) from exception
    if result.returncode != 0:
        raise PackError(
            "Unable to inspect the HarmonyOS native library dependencies.",
            1,
        )
    dependencies = {
        line.strip()
        for line in result.stdout.splitlines()
        if re.fullmatch(r"lib[^\s]+\.so", line.strip()) is not None
    }
    if not dependencies:
        raise PackError(
            "The HarmonyOS native library dependency list is empty.",
            1,
        )
    return dependencies


def validate_hap_native_dependencies(
    context: PackContext,
    hap: pathlib.Path,
) -> None:
    dependencies = hap_native_dependencies(context, hap)
    desktop_libraries = {
        "libnative_window_manager.so",
        "libnative_display_manager.so",
        "libohinput.so",
    }
    required = {"libEGL.so"}
    forbidden: set[str]
    if context.device_form == "mobile":
        required.add("libGLESv2.so")
        forbidden = desktop_libraries | {"libGLv4.so"}
    else:
        required.update(desktop_libraries)
        if context.graphics_api == "opengl":
            required.add("libGLv4.so")
            forbidden = {"libGLESv2.so"}
        else:
            required.add("libGLESv2.so")
            forbidden = {"libGLv4.so"}
    missing = sorted(required - dependencies)
    unexpected = sorted(forbidden & dependencies)
    if missing or unexpected:
        details: list[str] = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if unexpected:
            details.append("unexpected " + ", ".join(unexpected))
        raise PackError(
            "HarmonyOS native dependencies do not match the selected graphics backend: "
            + "; ".join(details),
            1,
        )


def build_hap(
    context: PackContext,
    signed: bool = False,
    device: HarmonyDevice | None = None,
) -> pathlib.Path:
    if signed:
        if device is None:
            raise PackError("A HarmonyOS device is required for signed export.", EXIT_DEVICE)
        ensure_harmony_device_connected(context, device)
    environment = os.environ.copy()
    environment["JAVA_HOME"] = str(context.tools.java_home)
    environment["NODE_HOME"] = str(context.tools.node_home)
    environment["DEVECO_SDK_HOME"] = str(context.tools.sdk_home)
    command = [
        str(context.tools.hvigor),
        "assembleHap",
        "--mode",
        "module",
        "-p",
        "module=entry@default",
        "-p",
        "product=default",
        "-p",
        "buildMode=release",
        "--no-parallel",
        "--no-daemon",
    ]
    output_dir = (
        context.stage_dir
        / "entry"
        / "build"
        / "default"
        / "outputs"
        / "default"
    )
    unsigned_hap = output_dir / "entry-default-unsigned.hap"
    signed_hap = output_dir / "entry-default-signed.hap"
    native_log_baseline = native_build_log_states(context.stage_dir)
    result = subprocess.run(command, cwd=context.stage_dir, env=environment, check=False)
    if result.returncode != 0:
        print_native_build_diagnostic(context.stage_dir, native_log_baseline)
        if signed and unsigned_hap.is_file() and not signed_hap.is_file():
            raise PackError(
                "DevEco Studio built the unsigned HAP but failed to sign it.",
                EXIT_SIGNING,
            )
        raise PackError(
            "DevEco Studio failed to build the HarmonyOS HAP.",
            result.returncode or 1,
        )
    validate_native_build_contract(context)
    signature = "signed" if signed else "unsigned"
    hap = signed_hap if signed else unsigned_hap
    if not hap.is_file():
        exit_code = EXIT_SIGNING if signed else 1
        raise PackError(f"{signature.title()} HarmonyOS HAP was not produced: {hap}", exit_code)
    with zipfile.ZipFile(hap) as archive:
        names = set(archive.namelist())
    if "libs/arm64-v8a/libentry.so" not in names:
        raise PackError("HarmonyOS HAP does not contain arm64-v8a/libentry.so.", 1)
    validate_hap_native_dependencies(context, hap)
    if signed:
        if device is None:
            raise PackError("A HarmonyOS device is required for signed export.", EXIT_DEVICE)
        ensure_harmony_device_connected(context, device)
        verify_signed_hap(context, hap, device)
    context.dist_dir.mkdir(parents=True, exist_ok=True)
    output = (
        context.dist_dir
        / f"{context.artifact_name}-harmony-{harmony_artifact_variant(context)}-{signature}.hap"
    )
    shutil.copy2(hap, output)
    return output


def harmony_app_is_foreground(
    context: PackContext,
    device: HarmonyDevice,
    timeout: float,
) -> bool:
    if timeout <= 0:
        return False
    try:
        result = subprocess.run(
            [
                str(context.tools.hdc),
                "-t",
                device.identifier,
                "shell",
                "aa",
                "dump",
                "-l",
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    if result.returncode != 0:
        return False
    for record in re.split(r"(?=^[ \t]*AbilityRecord ID #)", result.stdout, flags=re.MULTILINE):
        if f"bundle name [{context.bundle_name}]" not in record:
            continue
        if "main name [EntryAbility]" not in record:
            continue
        if re.search(r"^[ \t]*state #FOREGROUND(?:[ \t]|$)", record, re.MULTILINE) is None:
            continue
        app_state = re.search(r"^[ \t]*app state #([A-Z]+)(?:[ \t]|$)", record, re.MULTILINE)
        if app_state is not None and app_state.group(1) != "FOREGROUND":
            continue
        ready = re.search(r"^[ \t]*ready #([01])(?:[ \t]|$)", record, re.MULTILINE)
        if ready is not None and ready.group(1) != "1":
            continue
        return True
    return False


def wait_for_harmony_app_foreground(context: PackContext, device: HarmonyDevice) -> bool:
    deadline = time.monotonic() + HARMONY_LAUNCH_FOREGROUND_TIMEOUT_SECONDS
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        if harmony_app_is_foreground(
            context,
            device,
            min(HARMONY_LAUNCH_QUERY_TIMEOUT_SECONDS, remaining),
        ):
            return True
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        time.sleep(min(HARMONY_LAUNCH_POLL_SECONDS, remaining))


def launch_harmony_app(context: PackContext, device: HarmonyDevice) -> tuple[bool, str]:
    try:
        launch = subprocess.run(
            [
                str(context.tools.hdc),
                "-t",
                device.identifier,
                "shell",
                "aa",
                "start",
                "-a",
                "EntryAbility",
                "-b",
                context.bundle_name,
                "-m",
                "entry",
                "-W",
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=60,
        )
    except OSError as exception:
        return False, str(exception)
    except subprocess.TimeoutExpired:
        return False, "aa start timed out."
    diagnostic = redact_native_build_diagnostic(
        "\n".join(part.strip() for part in (launch.stdout, launch.stderr) if part.strip())
    )
    if len(diagnostic) > HARMONY_LAUNCH_DIAGNOSTIC_LIMIT:
        diagnostic = diagnostic[:HARMONY_LAUNCH_DIAGNOSTIC_LIMIT].rstrip() + "\n... launch output truncated ..."
    return (
        launch.returncode == 0 and wait_for_harmony_app_foreground(context, device),
        diagnostic,
    )


def install_and_launch_harmony_hap(
    context: PackContext,
    device: HarmonyDevice,
    hap: pathlib.Path,
) -> None:
    ensure_harmony_device_connected(context, device)
    print("Installing the signed HAP on the HarmonyOS device.", flush=True)
    try:
        install = subprocess.run(
            [str(context.tools.hdc), "-t", device.identifier, "install", "-r", str(hap)],
            check=False,
            capture_output=True,
            timeout=180,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError("Unable to install the HarmonyOS HAP.", EXIT_DEVICE) from exception
    if install.returncode != 0:
        raise PackError(
            "Failed to install the HarmonyOS HAP. Unlock the device and verify Developer Mode.",
            EXIT_DEVICE,
        )
    ensure_harmony_device_connected(context, device)
    print("Launching the HarmonyOS app.", flush=True)
    launch_diagnostic = ""
    for attempt in range(HARMONY_LAUNCH_ATTEMPTS):
        ensure_harmony_device_connected(context, device)
        launched, launch_diagnostic = launch_harmony_app(context, device)
        if launched:
            print("The HarmonyOS app is in the foreground.", flush=True)
            return
        if attempt + 1 < HARMONY_LAUNCH_ATTEMPTS:
            print("The app did not reach the foreground; retrying launch.", flush=True)
    message = "The HarmonyOS HAP was installed, but the app did not reach the foreground."
    if launch_diagnostic:
        message += "\n" + launch_diagnostic
    raise PackError(message, EXIT_DEVICE)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ScriptTools harmony-pack")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--export-to-device", action="store_true")
    parser.add_argument("--compile-lua", action="store_true")
    parser.add_argument("--encrypt-shaders", action="store_true")
    parser.add_argument("--encrypt-data", action="store_true")
    parser.add_argument("--encrypt-saves", action="store_true")
    parser.add_argument("--use-ldpak", action="store_true")
    parser.add_argument("--device-form", choices=("mobile", "2in1"), default="mobile")
    parser.add_argument("--graphics-api", choices=("opengl", "opengl-es"))
    parser.add_argument("project_folder", type=pathlib.Path)
    parser.add_argument("dist_folder", type=pathlib.Path, nargs="?")
    return parser


def main(arguments: list[str] | None = None) -> int:
    parser = create_parser()
    parsed = parser.parse_args(arguments)
    try:
        context = create_context(parsed)
        print(f"DevEco Studio: {context.tools.app}")
        print(f"Game name: {context.game_name}")
        print(f"Bundle name: {context.bundle_name}")
        print(f"Device form: {context.device_form}")
        print(f"Graphics API: {context.graphics_api}")
        device: HarmonyDevice | None = None
        if parsed.export_to_device:
            require_device_export_tools(context.tools)
            device = select_harmony_device(context.tools, context.device_form)
            print(
                f"One matching HarmonyOS {context.device_form} device was found "
                f"({device.device_type})."
            )
        if parsed.check:
            print(
                f"HarmonyOS {harmony_artifact_variant(context)} packaging check passed."
            )
            return 0
        if parsed.export_to_device:
            if device is None:
                raise PackError("A HarmonyOS device is required for device export.", EXIT_DEVICE)
            device = read_device_udid(context.tools, device)
            ensure_harmony_device_connected(context, device)
            overlay = resolve_signing_overlay(context, device)
            runtime_hash = prepare_stage(context)
            validate_generated_project(context, context.stage_dir)
            inject_signing_overlay(context, context.stage_dir, overlay)
            print(f"Runtime archive SHA-256: {runtime_hash}")
            try:
                output = build_hap(context, signed=True, device=device)
            except PackError as exception:
                if exception.exit_code == EXIT_SIGNING:
                    invalidate_signing_overlay(context, overlay)
                raise
            print(f"Signed pack complete: {output}")
            install_and_launch_harmony_hap(context, device, output)
            print(f"Installed and launched {context.game_name} on the HarmonyOS device.")
            return 0
        runtime_hash = prepare_stage(context)
        validate_generated_project(context, context.stage_dir)
        print(f"Runtime archive SHA-256: {runtime_hash}")
        output = build_hap(context)
        print(f"Pack complete: {output}")
        return 0
    except PackError as exception:
        print(str(exception), file=sys.stderr)
        return exception.exit_code
    except LdPakError as exception:
        print(str(exception), file=sys.stderr)
        return EXIT_PROJECT
    except (OSError, RuntimeError, zipfile.BadZipFile) as exception:
        print(str(exception), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
