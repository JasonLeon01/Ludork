from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import unicodedata
import xml.etree.ElementTree as ElementTree
import zipfile
from dataclasses import dataclass
from typing import TextIO

from ScriptTools.compile_lua import compile_scripts, resolve_luac
from ScriptTools.finalize_package import finalize_package


EXIT_TOOLCHAIN = 20
EXIT_SIGNING = 22
EXIT_PROJECT = 23
EXIT_APP_NAME_UNCHANGED = 24
ANDROID_COMPILE_SDK = 36
ANDROID_TARGET_SDK = 36
ANDROID_MIN_SDK = 24
ANDROID_BUILD_TOOLS = "36.0.0"
ANDROID_ABI = "arm64-v8a"
ANDROID_STL = "c++_static"
ANDROID_ACTIVITY_NAME = "com.ludork.android.LudorkActivity"
ANDROID_APP_CATEGORY = "game"
ANDROID_APP_CATEGORY_VALUE = 0
ANDROID_SCREEN_ORIENTATION = "sensorLandscape"
ANDROID_SCREEN_ORIENTATION_VALUE = 6
ANDROID_XML_NAMESPACE = "http://schemas.android.com/apk/res/android"
MINIMUM_NDK_MAJOR = 27
MINIMUM_CMAKE_VERSION = (3, 28, 0)
MINIMUM_JAVA_MAJOR = 17
ANDROID_GRADLE_PLUGIN_VERSION = "9.3.0"
GRADLE_VERSION = "9.5.0"
ANDROID_STUDIO_CANDIDATES = (
    pathlib.Path("/Applications/Android Studio.app"),
    pathlib.Path.home() / "Applications" / "Android Studio.app",
)
DEFAULT_APP_NAME_PATTERN = re.compile(
    r"^[ \t]*local[ \t]+APP_NAME[ \t]*=[ \t]*[\"']LudorkSample[\"'][ \t]*(?:--[^\r\n]*)?\r?$",
    re.MULTILINE,
)
TEMPLATE_TOKEN_PATTERN = re.compile(r"__LUDORK_[A-Z0-9_]+__")
HEX_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
NDK_REVISION_PATTERN = re.compile(
    r"^(?P<numbers>[0-9]+(?:\.[0-9]+){1,3})(?P<suffix>.*)$"
)
OPTIONAL_RUNTIME_LEGAL_FILES = (
    "LICENSE.md",
    "THIRD_PARTY_NOTICES.md",
    "THIRD_PARTY_NOTICES_zh_CN.md",
)
DEPENDENCY_NAMES = (
    "flac",
    "freetype",
    "harfbuzz",
    "libssh2",
    "mbedtls",
    "ogg",
    "sheenbidi",
    "vorbis",
)
SIGNING_STORE_PASSWORD_ENVIRONMENT = "LUDORK_ANDROID_STORE_PASSWORD"
SIGNING_KEY_PASSWORD_ENVIRONMENT = "LUDORK_ANDROID_KEY_PASSWORD"


class PackError(RuntimeError):
    def __init__(self, message: str, exit_code: int = 1) -> None:
        super().__init__(message)
        self.exit_code = exit_code


@dataclass(frozen=True)
class AndroidStudio:
    app: pathlib.Path
    java_home: pathlib.Path


@dataclass(frozen=True)
class AndroidSdk:
    root: pathlib.Path
    aapt: pathlib.Path
    aapt2: pathlib.Path
    zipalign: pathlib.Path
    apksigner: pathlib.Path


@dataclass(frozen=True, repr=False)
class AndroidSigningOptions:
    keystore_path: pathlib.Path
    key_alias: str
    keystore_password: str
    key_password: str


@dataclass(frozen=True)
class AndroidNdk:
    root: pathlib.Path
    revision: str
    version: tuple[int, ...]
    preview: bool
    toolchain: pathlib.Path
    clang: pathlib.Path
    clangxx: pathlib.Path
    nm: pathlib.Path
    strip: pathlib.Path
    readelf: pathlib.Path


@dataclass(frozen=True)
class CMakeTool:
    executable: pathlib.Path
    version: tuple[int, int, int]


@dataclass(frozen=True)
class RuntimeManifest:
    digest: str
    files: tuple[dict[str, object], ...]

    def as_dict(self) -> dict[str, object]:
        return {"version": 1, "hash": self.digest, "files": list(self.files)}


@dataclass(frozen=True)
class PackContext:
    project_dir: pathlib.Path
    dist_dir: pathlib.Path
    build_dir: pathlib.Path
    runtime_dir: pathlib.Path
    native_dir: pathlib.Path
    native_output_dir: pathlib.Path
    stage_dir: pathlib.Path
    template_dir: pathlib.Path
    script_tools: pathlib.Path
    studio: AndroidStudio
    sdk: AndroidSdk
    ndk: AndroidNdk
    cmake: CMakeTool
    make: pathlib.Path
    game_name: str
    artifact_name: str
    application_id: str
    use_luac: bool
    encrypt_shaders: bool
    encrypt_data: bool

    @property
    def environment(self) -> dict[str, str]:
        environment = os.environ.copy()
        environment.pop(SIGNING_STORE_PASSWORD_ENVIRONMENT, None)
        environment.pop(SIGNING_KEY_PASSWORD_ENVIRONMENT, None)
        environment["JAVA_HOME"] = str(self.studio.java_home)
        environment["ANDROID_SDK_ROOT"] = str(self.sdk.root)
        environment["PATH"] = (
            str(self.studio.java_home / "bin")
            + os.pathsep
            + environment.get("PATH", "")
        )
        environment["LANG"] = environment.get("LANG") or "en_US.UTF-8"
        return environment


def read_signing_options(
    arguments: argparse.Namespace,
    input_stream: TextIO,
) -> AndroidSigningOptions | None:
    keystore = arguments.keystore
    key_alias = arguments.key_alias
    if not arguments.sign:
        if keystore is not None or key_alias is not None:
            raise PackError(
                "Android signing arguments require --sign.",
                EXIT_SIGNING,
            )
        return None
    if keystore is None or key_alias is None:
        raise PackError(
            "Android signing requires a keystore and key alias.",
            EXIT_SIGNING,
        )
    if not keystore.is_absolute():
        raise PackError("The Android signing keystore path must be absolute.", EXIT_SIGNING)
    try:
        keystore_path = keystore.resolve(strict=True)
    except (OSError, RuntimeError) as exception:
        raise PackError("The Android signing keystore is unavailable.", EXIT_SIGNING) from exception
    if not keystore_path.is_file() or not os.access(keystore_path, os.R_OK):
        raise PackError("The Android signing keystore is unavailable.", EXIT_SIGNING)
    if not key_alias.strip() or "\r" in key_alias or "\n" in key_alias:
        raise PackError("The Android signing key alias is invalid.", EXIT_SIGNING)
    try:
        password_input = input_stream.read()
    except (OSError, UnicodeError) as exception:
        raise PackError("Unable to read Android signing passwords.", EXIT_SIGNING) from exception
    if password_input.endswith("\n"):
        password_input = password_input[:-1]
    passwords = password_input.split("\n")
    if (
        len(passwords) != 2
        or not passwords[0]
        or not passwords[1]
        or "\r" in passwords[0]
        or "\r" in passwords[1]
    ):
        raise PackError(
            "Android signing passwords must be two non-empty UTF-8 lines.",
            EXIT_SIGNING,
        )
    return AndroidSigningOptions(
        keystore_path,
        key_alias,
        passwords[0],
        passwords[1],
    )


def redact_signing_diagnostic(
    diagnostic: str,
    signing: AndroidSigningOptions,
) -> str:
    sensitive_values = {
        str(signing.keystore_path),
        signing.key_alias,
        signing.keystore_password,
        signing.key_password,
    }
    redacted = diagnostic
    for value in sorted(sensitive_values, key=len, reverse=True):
        if value:
            redacted = redacted.replace(value, "[REDACTED]")
    return redacted


def _required_executable(path: pathlib.Path, description: str) -> pathlib.Path:
    if not path.is_file() or not os.access(path, os.X_OK):
        raise PackError(f"{description} was not found or is not executable: {path}", EXIT_TOOLCHAIN)
    return path


def resolve_android_studio() -> AndroidStudio:
    app = next((candidate for candidate in ANDROID_STUDIO_CANDIDATES if candidate.is_dir()), None)
    if app is None:
        locations = "\n".join(str(candidate) for candidate in ANDROID_STUDIO_CANDIDATES)
        raise PackError(
            "Android Studio was not found in either supported location:\n" + locations,
            EXIT_TOOLCHAIN,
        )
    executable = app / "Contents" / "MacOS" / "studio"
    java_home = app / "Contents" / "jbr" / "Contents" / "Home"
    java = java_home / "bin" / "java"
    missing = [
        str(path)
        for path in (executable, java)
        if not path.is_file() or not os.access(path, os.X_OK)
    ]
    if missing:
        raise PackError(
            "Android Studio installation is incomplete:\n" + "\n".join(missing),
            EXIT_TOOLCHAIN,
        )
    try:
        result = subprocess.run(
            [str(java), "-version"],
            check=False,
            capture_output=True,
            text=True,
            timeout=15,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError(
            f"Android Studio JBR could not be started: {java} ({exception})",
            EXIT_TOOLCHAIN,
        ) from exception
    if result.returncode != 0:
        raise PackError(f"Android Studio JBR could not be started: {java}", EXIT_TOOLCHAIN)
    try:
        java_major = parse_java_major(result.stdout + "\n" + result.stderr)
    except ValueError as exception:
        raise PackError(
            f"Unable to read the Android Studio JBR version: {java}",
            EXIT_TOOLCHAIN,
        ) from exception
    if java_major < MINIMUM_JAVA_MAJOR:
        raise PackError(
            f"Android Studio JBR {MINIMUM_JAVA_MAJOR} or newer is required; "
            f"found Java {java_major}: {java}",
            EXIT_TOOLCHAIN,
        )
    return AndroidStudio(app.resolve(), java_home.resolve())


def parse_java_major(output: str) -> int:
    match = re.search(r'(?:java|openjdk) version "(?:(?:1\.)?)([0-9]+)', output)
    if match is None:
        raise ValueError("Unable to parse the Java version")
    return int(match.group(1))


def resolve_android_sdk_root() -> pathlib.Path:
    candidates: list[pathlib.Path] = []
    for name in ("ANDROID_SDK_ROOT", "ANDROID_HOME"):
        configured = os.environ.get(name, "").strip()
        if configured:
            candidates.append(pathlib.Path(configured).expanduser())
    candidates.append(pathlib.Path.home() / "Library" / "Android" / "sdk")
    root = next((candidate for candidate in candidates if candidate.is_dir()), None)
    if root is None:
        raise PackError(
            "Android SDK was not found. Install SDK Platform 36 and Build Tools 36.0.0 with Android Studio.",
            EXIT_TOOLCHAIN,
        )
    return root.resolve()


def android_sdk_at(root: pathlib.Path) -> AndroidSdk:
    platform_jar = root / "platforms" / f"android-{ANDROID_COMPILE_SDK}" / "android.jar"
    build_tools = root / "build-tools" / ANDROID_BUILD_TOOLS
    aapt = build_tools / "aapt"
    aapt2 = build_tools / "aapt2"
    zipalign = build_tools / "zipalign"
    apksigner = build_tools / "apksigner"
    missing: list[pathlib.Path] = []
    if not platform_jar.is_file():
        missing.append(platform_jar)
    for path in (aapt, aapt2, zipalign, apksigner):
        if not path.is_file() or not os.access(path, os.X_OK):
            missing.append(path)
    if missing:
        raise PackError(
            "Android SDK Platform 36 and Build Tools 36.0.0 are incomplete:\n"
            + "\n".join(str(path) for path in missing),
            EXIT_TOOLCHAIN,
        )
    return AndroidSdk(root, aapt, aapt2, zipalign, apksigner)


def resolve_android_sdk() -> AndroidSdk:
    return android_sdk_at(resolve_android_sdk_root())


def parse_cmake_version(output: str) -> tuple[int, int, int]:
    match = re.search(r"cmake version\s+([0-9]+)\.([0-9]+)\.([0-9]+)", output)
    if match is None:
        raise ValueError("Unable to parse the CMake version")
    return tuple(int(value) for value in match.groups())


def resolve_cmake() -> CMakeTool:
    configured = os.environ.get("LUDORK_CMAKE", "").strip()
    if configured:
        candidates = (pathlib.Path(configured).expanduser(),)
    else:
        candidates = tuple(
            pathlib.Path(candidate)
            for candidate in (
                shutil.which("cmake") or "",
                "/opt/homebrew/bin/cmake",
                "/usr/local/bin/cmake",
                "/Applications/CMake.app/Contents/bin/cmake",
            )
            if candidate
        )
    checked: set[pathlib.Path] = set()
    for candidate in candidates:
        candidate = candidate.resolve()
        if candidate in checked or not candidate.is_file() or not os.access(candidate, os.X_OK):
            continue
        checked.add(candidate)
        try:
            version_result = subprocess.run(
                [str(candidate), "--version"],
                check=False,
                capture_output=True,
                text=True,
                timeout=15,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        if version_result.returncode != 0:
            continue
        try:
            version = parse_cmake_version(version_result.stdout)
        except ValueError:
            continue
        if version < MINIMUM_CMAKE_VERSION:
            continue
        try:
            capabilities = subprocess.run(
                [str(candidate), "-E", "capabilities"],
                check=False,
                capture_output=True,
                text=True,
                timeout=15,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        try:
            data = json.loads(capabilities.stdout)
        except json.JSONDecodeError:
            continue
        generators = data.get("generators") if isinstance(data, dict) else None
        if capabilities.returncode != 0 or not isinstance(generators, list):
            continue
        if not any(
            isinstance(generator, dict) and generator.get("name") == "Unix Makefiles"
            for generator in generators
        ):
            continue
        return CMakeTool(candidate, version)
    minimum = ".".join(str(value) for value in MINIMUM_CMAKE_VERSION)
    raise PackError(
        f"System CMake {minimum} or newer with the Unix Makefiles generator was not found. "
        "Set LUDORK_CMAKE to the CMake executable.",
        EXIT_TOOLCHAIN,
    )


def resolve_make() -> pathlib.Path:
    return _required_executable(pathlib.Path("/usr/bin/make"), "Apple /usr/bin/make").resolve()


def parse_ndk_revision(text: str) -> tuple[str, tuple[int, ...], bool]:
    revision = ""
    for line in text.splitlines():
        key, separator, value = line.partition("=")
        if separator and key.strip() == "Pkg.Revision":
            revision = value.strip()
            break
    match = NDK_REVISION_PATTERN.fullmatch(revision)
    if match is None:
        raise ValueError("Pkg.Revision is missing or invalid")
    version = tuple(int(value) for value in match.group("numbers").split("."))
    preview = bool(match.group("suffix").strip())
    return revision, version, preview


def inspect_ndk(path: pathlib.Path) -> AndroidNdk:
    root = path.expanduser().resolve()
    properties = root / "source.properties"
    if not properties.is_file():
        raise ValueError("source.properties is missing")
    revision, version, preview = parse_ndk_revision(properties.read_text(encoding="utf-8"))
    if version[0] < MINIMUM_NDK_MAJOR:
        raise ValueError(f"NDK r{MINIMUM_NDK_MAJOR} or newer is required")
    toolchain = root / "build" / "cmake" / "android.toolchain.cmake"
    if not toolchain.is_file():
        raise ValueError("android.toolchain.cmake is missing")
    platforms_path = root / "meta" / "platforms.json"
    if not platforms_path.is_file():
        raise ValueError("meta/platforms.json is missing")
    try:
        platforms = json.loads(platforms_path.read_text(encoding="utf-8"))
        minimum = int(platforms["min"])
        maximum = int(platforms["max"])
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError) as exception:
        raise ValueError("meta/platforms.json is invalid") from exception
    if not minimum <= ANDROID_MIN_SDK <= maximum:
        raise ValueError(f"Android API {ANDROID_MIN_SDK} is not supported")
    prebuilt_roots = sorted(
        candidate
        for candidate in (root / "toolchains" / "llvm" / "prebuilt").glob("darwin-*")
        if candidate.is_dir()
    )
    for prebuilt in prebuilt_roots:
        binary_dir = prebuilt / "bin"
        clang = binary_dir / "clang"
        clangxx = binary_dir / "clang++"
        nm = binary_dir / "llvm-nm"
        strip = binary_dir / "llvm-strip"
        readelf = binary_dir / "llvm-readelf"
        tools = (clang, clangxx, nm, strip, readelf)
        if all(path.is_file() and os.access(path, os.X_OK) for path in tools):
            return AndroidNdk(
                root,
                revision,
                version,
                preview,
                toolchain.resolve(),
                clang,
                clangxx,
                nm,
                strip,
                readelf,
            )
    raise ValueError(
        "the Darwin LLVM toolchain is incomplete (clang, clang++, llvm-nm, "
        "llvm-readelf, and llvm-strip are required)"
    )


def resolve_android_ndk(sdk: AndroidSdk) -> AndroidNdk:
    ndk_root = sdk.root / "ndk"
    candidates: list[AndroidNdk] = []
    rejected: list[str] = []
    if ndk_root.is_dir():
        for path in sorted(ndk_root.iterdir()):
            if not path.is_dir() or path.name.startswith("."):
                continue
            try:
                candidate = inspect_ndk(path)
            except (OSError, ValueError) as exception:
                rejected.append(f"{path.name}: {exception}")
                continue
            if candidate.preview:
                rejected.append(f"{path.name}: preview revision {candidate.revision}")
                continue
            candidates.append(candidate)
    if not candidates:
        diagnostic = "\n".join(rejected) if rejected else "No NDK installations were found."
        raise PackError(
            f"No complete stable Android NDK r{MINIMUM_NDK_MAJOR} or newer was found under {ndk_root}. "
            "Install a stable NDK with Android Studio.\n" + diagnostic,
            EXIT_TOOLCHAIN,
        )
    return max(candidates, key=lambda candidate: candidate.version)


def resolve_script_tools() -> pathlib.Path:
    configured = os.environ.get("LUDORK_SCRIPT_TOOLS_EXECUTABLE", "").strip()
    path = pathlib.Path(configured or sys.argv[0]).expanduser().resolve()
    if not path.is_file():
        raise PackError(
            f"ScriptTools executable was not found: {path}. Run tools/init.sh first.",
            EXIT_TOOLCHAIN,
        )
    return path


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
            "Android packaging requires a C++ source project. Standalone projects are not supported.",
            EXIT_PROJECT,
        )
    required_directories = (
        "Assets",
        "Core",
        "Data",
        "include",
        "LuaSF",
        "lua-cjson",
        "Scripts",
        "Standard",
        "src",
        "zlib",
    )
    for name in required_directories:
        directory = project_dir / name
        if not directory.is_dir():
            raise PackError(f"Required Android project folder was not found: {directory}", EXIT_PROJECT)
    for relative in ("CMakeLists.txt", "cmake/Android/build.gradle.kts"):
        required = project_dir / relative
        if not required.is_file():
            raise PackError(f"Required Android project file was not found: {required}", EXIT_PROJECT)
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
    system_assets = project_dir / "Assets" / "System"
    if not any((system_assets / name).is_file() for name in ("icon.png", "icon.icns")):
        raise PackError(f"Project icon was not found in {system_assets}.", EXIT_PROJECT)
    if project_data.get("ffmpeg") is True:
        for required in (
            project_dir / "ffmpeg" / "configure",
            project_dir / "cmake" / "FFmpeg" / "build_android.sh",
        ):
            if not required.is_file():
                raise PackError(
                    f"The project enables FFmpeg but its Android build source is missing: {required}",
                    EXIT_PROJECT,
                )
    return project_dir


def read_game_name(project_dir: pathlib.Path) -> str:
    system_path = project_dir / "Data" / "Configs" / "System.json"
    try:
        data = json.loads(system_path.read_text(encoding="utf-8"))
        title = data["title"]["value"]
    except (OSError, json.JSONDecodeError, KeyError, TypeError) as exception:
        raise PackError(f"Unable to read game title from {system_path}: {exception}", EXIT_PROJECT) from exception
    if not isinstance(title, str) or not title.strip():
        raise PackError(f"Game title must be a non-empty string: {system_path}", EXIT_PROJECT)
    return title


def artifact_name(game_name: str) -> str:
    normalized = unicodedata.normalize("NFC", game_name)
    safe = re.sub(r'[\x00-\x1f\x7f<>:"/\\|?*;]+', "-", normalized)
    safe = re.sub(r"\s+", " ", safe).strip(" .")
    return (safe[:80].rstrip(" .") or "Ludork Game")


def application_id(game_name: str) -> str:
    ascii_name = (
        unicodedata.normalize("NFKD", game_name)
        .encode("ascii", "ignore")
        .decode("ascii")
        .lower()
    )
    slug = re.sub(r"[^a-z0-9]+", "", ascii_name)[:40] or "game"
    digest = hashlib.sha256(game_name.encode("utf-8")).hexdigest()[:10]
    return f"com.ludork.g{slug}.h{digest}"


def create_context(arguments: argparse.Namespace) -> PackContext:
    if sys.platform != "darwin" or platform.machine() != "arm64":
        raise PackError(
            "Android packaging currently requires Apple Silicon macOS.",
            EXIT_TOOLCHAIN,
        )
    project_dir = resolve_project(arguments.project_folder)
    dist_dir = (
        arguments.dist_folder.expanduser().resolve()
        if arguments.dist_folder is not None
        else project_dir / "dist"
    )
    errors: list[str] = []
    studio: AndroidStudio | None = None
    sdk: AndroidSdk | None = None
    sdk_root: pathlib.Path | None = None
    ndk: AndroidNdk | None = None
    cmake: CMakeTool | None = None
    make: pathlib.Path | None = None
    script_tools: pathlib.Path | None = None
    try:
        studio = resolve_android_studio()
    except PackError as exception:
        errors.append(str(exception))
    try:
        sdk_root = resolve_android_sdk_root()
    except PackError as exception:
        errors.append(str(exception))
    if sdk_root is not None:
        try:
            sdk = android_sdk_at(sdk_root)
        except PackError as exception:
            errors.append(str(exception))
        sdk_paths = AndroidSdk(
            sdk_root,
            sdk_root / "build-tools" / ANDROID_BUILD_TOOLS / "aapt",
            sdk_root / "build-tools" / ANDROID_BUILD_TOOLS / "aapt2",
            sdk_root / "build-tools" / ANDROID_BUILD_TOOLS / "zipalign",
            sdk_root / "build-tools" / ANDROID_BUILD_TOOLS / "apksigner",
        )
        try:
            ndk = resolve_android_ndk(sdk_paths)
        except PackError as exception:
            errors.append(str(exception))
    try:
        cmake = resolve_cmake()
    except PackError as exception:
        errors.append(str(exception))
    try:
        make = resolve_make()
    except PackError as exception:
        errors.append(str(exception))
    try:
        script_tools = resolve_script_tools()
    except PackError as exception:
        errors.append(str(exception))
    if errors:
        raise PackError(
            "Android toolchain preflight failed:\n\n" + "\n\n".join(errors),
            EXIT_TOOLCHAIN,
        )
    assert studio is not None
    assert sdk is not None
    assert ndk is not None
    assert cmake is not None
    assert make is not None
    assert script_tools is not None
    game_name = read_game_name(project_dir)
    build_dir = project_dir / "build" / "android"
    return PackContext(
        project_dir=project_dir,
        dist_dir=dist_dir,
        build_dir=build_dir,
        runtime_dir=build_dir / "runtime",
        native_dir=build_dir / "native" / ANDROID_ABI,
        native_output_dir=build_dir / "native-output" / ANDROID_ABI,
        stage_dir=build_dir / "gradle",
        template_dir=project_dir / "cmake" / "Android",
        script_tools=script_tools,
        studio=studio,
        sdk=sdk,
        ndk=ndk,
        cmake=cmake,
        make=make,
        game_name=game_name,
        artifact_name=artifact_name(game_name),
        application_id=application_id(game_name),
        use_luac=arguments.compile_lua,
        encrypt_shaders=arguments.encrypt_shaders,
        encrypt_data=arguments.encrypt_data,
    )


def copy_runtime_resources(context: PackContext) -> None:
    if context.runtime_dir.exists():
        shutil.rmtree(context.runtime_dir)
    context.runtime_dir.mkdir(parents=True)
    for name in ("Assets", "Data", "Scripts"):
        shutil.copytree(
            context.project_dir / name,
            context.runtime_dir / name,
            ignore=shutil.ignore_patterns(".DS_Store", "*.anim.json"),
        )
    licenses = context.project_dir / "Licenses"
    if licenses.is_dir():
        shutil.copytree(
            licenses,
            context.runtime_dir / "Licenses",
            ignore=shutil.ignore_patterns(".DS_Store"),
        )
    for name in OPTIONAL_RUNTIME_LEGAL_FILES:
        source = context.project_dir / name
        if source.is_file():
            shutil.copy2(source, context.runtime_dir / name)
    finalize_package(
        context.runtime_dir,
        context.encrypt_shaders,
        context.encrypt_data,
    )
    if context.use_luac:
        compile_scripts(context.runtime_dir / "Scripts", resolve_luac())


def create_runtime_manifest(runtime_dir: pathlib.Path) -> RuntimeManifest:
    entries: list[dict[str, object]] = []
    for path in sorted(
        (candidate for candidate in runtime_dir.rglob("*") if candidate.is_file()),
        key=lambda candidate: candidate.relative_to(runtime_dir).as_posix(),
    ):
        relative = path.relative_to(runtime_dir).as_posix()
        data = path.read_bytes()
        entries.append(
            {
                "path": relative,
                "size": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )
    required_prefixes = ("Assets/", "Data/", "Scripts/")
    for prefix in required_prefixes:
        if not any(str(entry["path"]).startswith(prefix) for entry in entries):
            raise PackError(f"Android runtime contains no files under {prefix.rstrip('/')}.", EXIT_PROJECT)
    entry_names = {str(entry["path"]) for entry in entries}
    if not ({"Scripts/Entry.lua", "Scripts/Entry.luac"} & entry_names):
        raise PackError("Android runtime is missing Scripts/Entry.lua or Scripts/Entry.luac.", EXIT_PROJECT)
    digest = runtime_manifest_digest(entries)
    return RuntimeManifest(digest, tuple(entries))


def runtime_manifest_digest(files: list[dict[str, object]]) -> str:
    digest = hashlib.sha256()
    for entry in files:
        digest.update(str(entry["path"]).encode("utf-8"))
        digest.update(b"\0")
        digest.update(str(entry["size"]).encode("ascii"))
        digest.update(b"\0")
        digest.update(str(entry["sha256"]).encode("ascii"))
        digest.update(b"\n")
    return digest.hexdigest()


def _kotlin_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def _properties_path(path: pathlib.Path) -> str:
    return str(path).replace("\\", "\\\\").replace(":", "\\:")


def replace_template_tokens(
    stage_dir: pathlib.Path,
    game_name: str,
    app_id: str,
) -> None:
    replacements = {
        "__LUDORK_APPLICATION_ID_LITERAL__": _kotlin_string(app_id),
        "__LUDORK_GAME_NAME_XML__": html.escape(game_name, quote=True),
    }
    text_names = {
        "build.gradle.kts",
        "settings.gradle.kts",
        "gradle.properties",
        "gradle-wrapper.properties",
        "AndroidManifest.xml",
    }
    for path in sorted(stage_dir.rglob("*")):
        if not path.is_file() or (path.name not in text_names and path.suffix != ".java"):
            continue
        text = path.read_text(encoding="utf-8")
        for token, value in replacements.items():
            text = text.replace(token, value)
        unresolved = TEMPLATE_TOKEN_PATTERN.findall(text)
        if unresolved:
            raise PackError(
                f"Unresolved Android template token in {path}: {', '.join(sorted(set(unresolved)))}",
                EXIT_PROJECT,
            )
        path.write_text(text, encoding="utf-8")


def create_app_icon(context: PackContext) -> None:
    destination = (
        context.stage_dir
        / "app"
        / "src"
        / "main"
        / "res"
        / "drawable-nodpi"
        / "app_icon.png"
    )
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
            [
                sips,
                "-s",
                "format",
                "png",
                "-z",
                "512",
                "512",
                str(icns),
                "--out",
                str(destination),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode == 0 and destination.is_file():
            return
    raise PackError(f"Unable to create the Android app icon from {system_assets}.", EXIT_PROJECT)


def prepare_gradle_stage(
    context: PackContext,
    manifest: RuntimeManifest,
) -> None:
    if context.stage_dir.exists():
        shutil.rmtree(context.stage_dir)
    shutil.copytree(context.template_dir, context.stage_dir)
    wrapper_source = (
        context.project_dir
        / "LuaSF"
        / "third_party"
        / "SFML"
        / "examples"
        / "projects"
        / "android"
    )
    wrapper_files = (
        pathlib.Path("gradlew"),
        pathlib.Path("gradlew.bat"),
        pathlib.Path("gradle/wrapper/gradle-wrapper.jar"),
    )
    for relative in wrapper_files:
        source = wrapper_source / relative
        if not source.is_file():
            raise PackError(f"SFML Android Gradle wrapper file was not found: {source}", EXIT_PROJECT)
        destination = context.stage_dir / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    os.chmod(context.stage_dir / "gradlew", 0o755)
    assets_dir = context.stage_dir / "app" / "src" / "main" / "assets"
    assets_dir.mkdir(parents=True, exist_ok=True)
    for source in sorted(context.runtime_dir.iterdir(), key=lambda path: path.name):
        destination = assets_dir / source.name
        if source.is_dir():
            shutil.copytree(source, destination)
        else:
            shutil.copy2(source, destination)
    manifest_path = assets_dir / "ludork-runtime-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest.as_dict(), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    local_properties = context.stage_dir / "local.properties"
    local_properties.write_text(
        f"sdk.dir={_properties_path(context.sdk.root)}\n",
        encoding="utf-8",
    )
    create_app_icon(context)
    replace_template_tokens(context.stage_dir, context.game_name, context.application_id)


def cached_dependency_arguments(project_dir: pathlib.Path) -> list[str]:
    roots = (
        project_dir / "build" / "android" / "_deps",
        project_dir / "build" / "_deps",
        project_dir / "build" / "Release" / "_deps",
        project_dir / "build" / "Debug" / "_deps",
    )
    arguments: list[str] = []
    for name in DEPENDENCY_NAMES:
        for root in roots:
            source = root / f"{name}-src"
            if (source / "CMakeLists.txt").is_file():
                arguments.append(f"-DFETCHCONTENT_SOURCE_DIR_{name.upper()}={source}")
                break
    return arguments


def native_configure_command(
    context: PackContext,
    runtime_hash: str,
) -> list[str]:
    command = [
        str(context.cmake.executable),
        "-S",
        str(context.project_dir),
        "-B",
        str(context.native_dir),
        "-G",
        "Unix Makefiles",
        f"-DCMAKE_MAKE_PROGRAM={context.make}",
        f"-DCMAKE_TOOLCHAIN_FILE={context.ndk.toolchain}",
        f"-DANDROID_NDK={context.ndk.root}",
        "-DCMAKE_SYSTEM_NAME=Android",
        f"-DANDROID_ABI={ANDROID_ABI}",
        f"-DANDROID_PLATFORM=android-{ANDROID_MIN_SDK}",
        f"-DANDROID_STL={ANDROID_STL}",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DSFML_BUILD_EXAMPLES=OFF",
        "-DSFML_BUILD_TEST_SUITE=OFF",
        "-DSFML_BUILD_DOC=OFF",
        "-DSFML_USE_SYSTEM_DEPS=OFF",
        "-DLUASF_BUILD_SHARED_SFML=OFF",
        "-DLUASF_GENERATE_LUA_STUB=OFF",
        f"-DLUDORK_RUNTIME_OUTPUT_DIRECTORY={context.native_output_dir}",
        f"-DLUDORK_ANDROID_RUNTIME_HASH={runtime_hash}",
        f"-DLUDORK_SCRIPT_TOOLS_EXECUTABLE={context.script_tools}",
    ]
    command.extend(cached_dependency_arguments(context.project_dir))
    return command


def _run_streaming(
    command: list[str],
    cwd: pathlib.Path,
    environment: dict[str, str] | None = None,
) -> None:
    result = subprocess.run(command, cwd=cwd, env=environment, check=False)
    if result.returncode != 0:
        raise PackError(
            f"Command failed with exit code {result.returncode}: {command[0]}",
            result.returncode or 1,
        )


def build_native_library(context: PackContext, runtime_hash: str) -> pathlib.Path:
    for directory in (context.native_dir, context.native_output_dir):
        if directory.exists():
            shutil.rmtree(directory)
        directory.mkdir(parents=True)
    _run_streaming(native_configure_command(context, runtime_hash), context.project_dir)
    _run_streaming(
        [
            str(context.cmake.executable),
            "--build",
            str(context.native_dir),
            "--target",
            "Main",
            "--parallel",
            str(max(1, os.cpu_count() or 1)),
        ],
        context.project_dir,
    )
    expected = context.native_output_dir / "Release" / "libludork.so"
    candidates = [expected] if expected.is_file() else []
    if not candidates:
        candidates = sorted(
            path
            for path in context.native_output_dir.rglob("libludork.so")
            if path.is_file()
        )
    if len(candidates) != 1:
        raise PackError(
            "Android native build did not produce exactly one libludork.so under "
            f"{context.native_output_dir}.",
        )
    destination = (
        context.stage_dir
        / "app"
        / "src"
        / "main"
        / "jniLibs"
        / ANDROID_ABI
        / "libludork.so"
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(candidates[0], destination)
    result = subprocess.run(
        [str(context.ndk.strip), "--strip-unneeded", str(destination)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise PackError(
            "NDK llvm-strip failed for libludork.so.\n"
            + (result.stderr or result.stdout).strip()
        )
    return destination


def build_unsigned_apk(context: PackContext) -> pathlib.Path:
    _run_streaming(
        [
            str(context.stage_dir / "gradlew"),
            "--no-daemon",
            "--console=plain",
            ":app:lintRelease",
            ":app:assembleRelease",
        ],
        context.stage_dir,
        context.environment,
    )
    apk = (
        context.stage_dir
        / "app"
        / "build"
        / "outputs"
        / "apk"
        / "release"
        / "app-release-unsigned.apk"
    )
    if not apk.is_file():
        raise PackError(f"Gradle did not produce the unsigned Release APK: {apk}")
    return apk


def _valid_runtime_path(value: object) -> bool:
    if not isinstance(value, str) or not value or "\\" in value or value.startswith("/"):
        return False
    parts = value.split("/")
    return bool(parts) and all(part not in ("", ".", "..") for part in parts)


def validate_runtime_manifest(value: object) -> dict[str, object]:
    if not isinstance(value, dict) or set(value) != {"version", "hash", "files"}:
        raise PackError("The APK runtime manifest has an invalid top-level shape.")
    if value.get("version") != 1:
        raise PackError("The APK runtime manifest version is not 1.")
    digest = value.get("hash")
    if not isinstance(digest, str) or HEX_SHA256_PATTERN.fullmatch(digest) is None:
        raise PackError("The APK runtime manifest has an invalid hash.")
    files = value.get("files")
    if not isinstance(files, list) or not files:
        raise PackError("The APK runtime manifest has no file entries.")
    previous = ""
    seen: set[str] = set()
    for entry in files:
        if not isinstance(entry, dict) or set(entry) != {"path", "size", "sha256"}:
            raise PackError("The APK runtime manifest contains an invalid file entry.")
        path = entry.get("path")
        size = entry.get("size")
        sha256 = entry.get("sha256")
        if not _valid_runtime_path(path):
            raise PackError(f"The APK runtime manifest contains an unsafe path: {path!r}")
        assert isinstance(path, str)
        if path in seen or (previous and path <= previous):
            raise PackError("The APK runtime manifest paths are duplicated or not sorted.")
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise PackError(f"The APK runtime manifest has an invalid size for {path}.")
        if not isinstance(sha256, str) or HEX_SHA256_PATTERN.fullmatch(sha256) is None:
            raise PackError(f"The APK runtime manifest has an invalid SHA-256 for {path}.")
        previous = path
        seen.add(path)
    if runtime_manifest_digest(files) != digest:
        raise PackError("The APK runtime manifest aggregate hash does not match its file list.")
    return value


def validate_apk_archive(
    apk: pathlib.Path,
    expected_manifest: RuntimeManifest,
    *,
    signed: bool = False,
) -> None:
    try:
        with zipfile.ZipFile(apk) as archive:
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise PackError("The APK contains duplicate ZIP entries.")
            for name in names:
                if not _valid_runtime_path(name.rstrip("/")):
                    raise PackError(f"The APK contains an unsafe ZIP path: {name}")
            name_set = set(names)
            required = {
                "AndroidManifest.xml",
                "classes.dex",
                f"lib/{ANDROID_ABI}/libludork.so",
                "assets/ludork-runtime-manifest.json",
            }
            missing = sorted(required - name_set)
            if missing:
                raise PackError("The APK is missing required entries:\n" + "\n".join(missing))
            native_libraries = {
                name
                for name in names
                if name.startswith("lib/") and name.endswith(".so")
            }
            expected_library = {f"lib/{ANDROID_ABI}/libludork.so"}
            if native_libraries != expected_library:
                raise PackError(
                    "The APK must contain only arm64-v8a/libludork.so; found: "
                    + ", ".join(sorted(native_libraries))
                )
            forbidden = [
                name
                for name in names
                if pathlib.PurePosixPath(name.rstrip("/")).name == ".DS_Store"
                or name.endswith(".d.lua")
                or name.endswith(".anim.json")
                or "/Scripts/stub/" in "/" + name
                or not signed
                and name.startswith("META-INF/")
                and name.upper().endswith((".RSA", ".DSA", ".EC", ".SF"))
            ]
            if forbidden:
                raise PackError(
                    "The APK contains forbidden development or signing entries:\n"
                    + "\n".join(sorted(forbidden))
                )
            manifest_value = validate_runtime_manifest(
                json.loads(
                    archive.read("assets/ludork-runtime-manifest.json").decode("utf-8")
                )
            )
            if manifest_value != expected_manifest.as_dict():
                raise PackError("The APK runtime manifest differs from the staged manifest.")
            manifest_files = manifest_value["files"]
            assert isinstance(manifest_files, list)
            runtime_names = {str(entry["path"]) for entry in manifest_files}
            expected_assets = {
                "assets/ludork-runtime-manifest.json",
                *("assets/" + name for name in runtime_names),
            }
            actual_assets = {
                name
                for name in names
                if name.startswith("assets/") and not name.endswith("/")
            }
            if actual_assets != expected_assets:
                unexpected = sorted(actual_assets - expected_assets)
                unbundled = sorted(expected_assets - actual_assets)
                detail: list[str] = []
                if unexpected:
                    detail.append("Unexpected APK assets:\n" + "\n".join(unexpected))
                if unbundled:
                    detail.append("Missing APK assets:\n" + "\n".join(unbundled))
                raise PackError("\n".join(detail))
            for prefix in ("Assets/", "Data/", "Scripts/"):
                if not any(name.startswith(prefix) for name in runtime_names):
                    raise PackError(f"The APK runtime manifest contains no {prefix.rstrip('/')} files.")
            if not ({"Scripts/Entry.lua", "Scripts/Entry.luac"} & runtime_names):
                raise PackError("The APK runtime manifest has no Lua entry script.")
            for entry in manifest_files:
                relative = str(entry["path"])
                archive_path = "assets/" + relative
                if archive_path not in name_set:
                    raise PackError(f"The APK is missing a manifested runtime asset: {relative}")
                data = archive.read(archive_path)
                if len(data) != entry["size"]:
                    raise PackError(f"The APK runtime asset size is invalid: {relative}")
                if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                    raise PackError(f"The APK runtime asset hash is invalid: {relative}")
    except (OSError, zipfile.BadZipFile, UnicodeDecodeError, json.JSONDecodeError) as exception:
        raise PackError(f"Unable to validate the APK archive: {exception}") from exception


def _run_capture(
    command: list[str],
    timeout: float = 120.0,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=environment,
        )
    except (OSError, subprocess.TimeoutExpired) as exception:
        raise PackError(f"Unable to run APK validation tool: {command[0]} ({exception})") from exception


def _run_signing_tool(
    command: list[str],
    signing: AndroidSigningOptions,
    *,
    input_text: str | None = None,
    environment: dict[str, str] | None = None,
    timeout: float = 120.0,
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            command,
            check=False,
            capture_output=True,
            input=input_text,
            encoding="utf-8",
            timeout=timeout,
            env=environment,
        )
    except (OSError, subprocess.TimeoutExpired, UnicodeError, ValueError) as exception:
        message = redact_signing_diagnostic(str(exception), signing)
        raise PackError(
            "Unable to run the Android signing tool.\n" + message,
            EXIT_SIGNING,
        ) from exception


def _signing_tool_diagnostic(
    result: subprocess.CompletedProcess[str],
    signing: AndroidSigningOptions,
) -> str:
    diagnostic = "\n".join(
        value.strip()
        for value in (result.stdout, result.stderr)
        if value and value.strip()
    )
    return redact_signing_diagnostic(diagnostic, signing)


def validate_signing_credentials(
    context: PackContext,
    signing: AndroidSigningOptions,
) -> None:
    keytool = context.studio.java_home / "bin" / "keytool"
    if not keytool.is_file() or not os.access(keytool, os.X_OK):
        raise PackError(
            "Android Studio JBR keytool is unavailable for APK signing.",
            EXIT_SIGNING,
        )
    environment = context.environment
    environment[SIGNING_STORE_PASSWORD_ENVIRONMENT] = signing.keystore_password
    environment[SIGNING_KEY_PASSWORD_ENVIRONMENT] = signing.key_password
    try:
        result = _run_signing_tool(
            [
                str(keytool),
                "-certreq",
                "-keystore",
                str(signing.keystore_path),
                "-alias",
                signing.key_alias,
                "-storepass:env",
                SIGNING_STORE_PASSWORD_ENVIRONMENT,
                "-keypass:env",
                SIGNING_KEY_PASSWORD_ENVIRONMENT,
            ],
            signing,
            environment=environment,
            timeout=30.0,
        )
    finally:
        environment.pop(SIGNING_STORE_PASSWORD_ENVIRONMENT, None)
        environment.pop(SIGNING_KEY_PASSWORD_ENVIRONMENT, None)
    if result.returncode != 0:
        diagnostic = _signing_tool_diagnostic(result, signing)
        message = "Android signing credentials could not be validated."
        if diagnostic:
            message += "\n" + diagnostic
        raise PackError(message, EXIT_SIGNING)


def validate_apk_metadata(
    context: PackContext,
    apk: pathlib.Path,
    *,
    signed: bool = False,
) -> None:
    badging = _run_capture([str(context.sdk.aapt), "dump", "badging", str(apk)])
    if badging.returncode != 0:
        raise PackError("aapt could not read the APK metadata.\n" + badging.stderr.strip())
    checks = (
        (f"package: name='{context.application_id}'", "application ID"),
        (f"sdkVersion:'{ANDROID_MIN_SDK}'", "minimum SDK"),
        (f"targetSdkVersion:'{ANDROID_TARGET_SDK}'", "target SDK"),
        (f"native-code: '{ANDROID_ABI}'", "native ABI"),
        (
            f"launchable-activity: name='{ANDROID_ACTIVITY_NAME}'",
            "launchable activity",
        ),
    )
    for expected, description in checks:
        if expected not in badging.stdout:
            raise PackError(f"The APK {description} is invalid; expected {expected!r}.")
    xmltree = _run_capture(
        [
            str(context.sdk.aapt),
            "dump",
            "xmltree",
            str(apk),
            "AndroidManifest.xml",
        ]
    )
    if xmltree.returncode != 0:
        raise PackError("aapt could not read the APK binary manifest.\n" + xmltree.stderr.strip())
    if not manifest_has_native_library_metadata(xmltree.stdout):
        raise PackError(
            "The APK manifest does not contain android.app.lib_name=ludork on the activity."
        )
    if not manifest_has_game_application_category(xmltree.stdout):
        raise PackError("The APK manifest does not identify the application as a game.")
    if not manifest_has_sensor_landscape_activity(xmltree.stdout):
        raise PackError("The APK launch activity does not request sensor-selected landscape.")
    alignment = _run_capture(
        [str(context.sdk.zipalign), "-c", "-P", "16", "-v", "4", str(apk)]
    )
    if alignment.returncode != 0:
        raise PackError("The APK does not pass 16 KiB zip alignment validation.")
    signature = _run_capture(
        [
            str(context.sdk.apksigner),
            "verify",
            "--verbose",
            "--min-sdk-version",
            str(ANDROID_MIN_SDK),
            str(apk),
        ],
        environment=context.environment,
    )
    signature_diagnostic = signature.stdout + "\n" + signature.stderr
    if signed:
        if signature.returncode != 0:
            raise PackError(
                "The signed APK signature is invalid.\n"
                + signature_diagnostic.strip()
            )
        return
    if signature.returncode == 0:
        raise PackError("The Release APK is unexpectedly signed; an unsigned APK was required.")
    if not is_expected_unsigned_apksigner_diagnostic(signature_diagnostic):
        raise PackError(
            "apksigner could not establish that the APK is complete and unsigned.\n"
            + signature_diagnostic.strip()
        )


def is_expected_unsigned_apksigner_diagnostic(diagnostic: str) -> bool:
    return (
        "DOES NOT VERIFY" in diagnostic
        and "Missing META-INF/MANIFEST.MF" in diagnostic
    )


def xmltree_direct_attribute_blocks(
    xmltree: str,
    element_name: str,
) -> list[list[str]]:
    blocks: list[list[str]] = []
    lines = xmltree.splitlines()
    element_pattern = re.compile(rf"^\s*E: {re.escape(element_name)}(?:\s|$)")
    for element_index, line in enumerate(lines):
        if element_pattern.match(line) is None:
            continue
        attributes: list[str] = []
        for candidate in lines[element_index + 1 :]:
            if re.match(r"^\s*E: ", candidate) is not None:
                break
            if re.match(r"^\s*A: ", candidate) is not None:
                attributes.append(candidate.strip())
        blocks.append(attributes)
    return blocks


def xmltree_string_attribute(
    attributes: list[str],
    name: str,
) -> str | None:
    pattern = re.compile(
        rf'^A: android:{re.escape(name)}(?:\(0x[0-9a-fA-F]+\))?="([^"]*)"'
    )
    for attribute in attributes:
        match = pattern.match(attribute)
        if match is not None:
            return match.group(1)
    return None


def xmltree_integer_attribute(
    attributes: list[str],
    name: str,
) -> int | None:
    pattern = re.compile(
        rf"^A: android:{re.escape(name)}(?:\(0x[0-9a-fA-F]+\))?="
        r"\(type 0x10\)0x([0-9a-fA-F]+)(?:\s|$)"
    )
    for attribute in attributes:
        match = pattern.match(attribute)
        if match is not None:
            return int(match.group(1), 16)
    return None


def manifest_has_game_application_category(xmltree: str) -> bool:
    for attributes in xmltree_direct_attribute_blocks(xmltree, "application"):
        category = xmltree_integer_attribute(attributes, "appCategory")
        if category == ANDROID_APP_CATEGORY_VALUE:
            return True
        if xmltree_string_attribute(attributes, "appCategory") == ANDROID_APP_CATEGORY:
            return True
    return False


def manifest_has_sensor_landscape_activity(xmltree: str) -> bool:
    for attributes in xmltree_direct_attribute_blocks(xmltree, "activity"):
        if xmltree_string_attribute(attributes, "name") != ANDROID_ACTIVITY_NAME:
            continue
        orientation = xmltree_integer_attribute(attributes, "screenOrientation")
        if orientation == ANDROID_SCREEN_ORIENTATION_VALUE:
            return True
        if (
            xmltree_string_attribute(attributes, "screenOrientation")
            == ANDROID_SCREEN_ORIENTATION
        ):
            return True
    return False


def manifest_has_native_library_metadata(xmltree: str) -> bool:
    lines = xmltree.splitlines()
    for activity_index, line in enumerate(lines):
        activity = re.match(r"^(\s*)E: activity(?:\s|$)", line)
        if activity is None:
            continue
        activity_indent = len(activity.group(1))
        index = activity_index + 1
        while index < len(lines):
            element = re.match(r"^(\s*)E: ([^\s]+)", lines[index])
            if element is not None and len(element.group(1)) <= activity_indent:
                break
            if element is None or element.group(2) != "meta-data":
                index += 1
                continue
            metadata_indent = len(element.group(1))
            metadata_lines = [lines[index]]
            index += 1
            while index < len(lines):
                sibling = re.match(r"^(\s*)E: ", lines[index])
                if sibling is not None and len(sibling.group(1)) <= metadata_indent:
                    break
                metadata_lines.append(lines[index])
                index += 1
            metadata = "\n".join(metadata_lines)
            if '="android.app.lib_name"' in metadata and '="ludork"' in metadata:
                return True
        continue
    return False


def unresolved_ffmpeg_symbols(nm_output: str) -> list[str]:
    symbols = {
        line.split()[-1].split("@", 1)[0]
        for line in nm_output.splitlines()
        if line.split()
    }
    return sorted(
        symbol
        for symbol in symbols
        if re.fullmatch(r"(?:av|sws|swr)[A-Za-z0-9_]*", symbol) is not None
    )


ANDROID_SYSTEM_LIBRARIES = {
    "libaaudio.so",
    "libamidi.so",
    "libandroid.so",
    "libcamera2ndk.so",
    "libc.so",
    "libdl.so",
    "libEGL.so",
    "libGLESv1_CM.so",
    "libGLESv2.so",
    "libGLESv3.so",
    "libjnigraphics.so",
    "liblog.so",
    "libm.so",
    "libmediandk.so",
    "libnativewindow.so",
    "libOpenMAXAL.so",
    "libOpenSLES.so",
    "libstdc++.so",
    "libsync.so",
    "libvulkan.so",
    "libz.so",
}


def validate_native_library(context: PackContext, apk: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory(prefix="ludork-android-verify-") as temporary:
        library = pathlib.Path(temporary) / "libludork.so"
        with zipfile.ZipFile(apk) as archive:
            library.write_bytes(archive.read(f"lib/{ANDROID_ABI}/libludork.so"))
        header = _run_capture([str(context.ndk.readelf), "-hW", str(library)])
        if header.returncode != 0 or "Machine:" not in header.stdout or "AArch64" not in header.stdout:
            raise PackError("libludork.so is not a valid AArch64 ELF library.")
        program_headers = _run_capture([str(context.ndk.readelf), "-lW", str(library)])
        if program_headers.returncode != 0:
            raise PackError("llvm-readelf could not read libludork.so program headers.")
        load_alignments: list[int] = []
        for line in program_headers.stdout.splitlines():
            fields = line.split()
            if fields and fields[0] == "LOAD":
                try:
                    load_alignments.append(int(fields[-1], 16))
                except ValueError as exception:
                    raise PackError("Unable to read libludork.so LOAD alignment.") from exception
        if not load_alignments or any(alignment < 0x4000 for alignment in load_alignments):
            raise PackError("libludork.so is not aligned for Android 16 KiB memory pages.")
        dynamic = _run_capture([str(context.ndk.readelf), "-dW", str(library)])
        if dynamic.returncode != 0:
            raise PackError("llvm-readelf could not read libludork.so dependencies.")
        dependencies = set(re.findall(r"\(NEEDED\).*\[([^\]]+)\]", dynamic.stdout))
        unexpected = sorted(dependencies - ANDROID_SYSTEM_LIBRARIES)
        if "libc++_shared.so" in dependencies or unexpected:
            raise PackError(
                "libludork.so has unexpected shared-library dependencies: "
                + ", ".join(unexpected or ["libc++_shared.so"])
            )
        symbols = _run_capture([str(context.ndk.nm), "-D", "--defined-only", str(library)])
        if symbols.returncode != 0 or "ANativeActivity_onCreate" not in symbols.stdout:
            raise PackError("libludork.so does not export ANativeActivity_onCreate.")
        undefined = _run_capture(
            [str(context.ndk.nm), "-D", "--undefined-only", str(library)]
        )
        if undefined.returncode != 0:
            raise PackError("llvm-nm could not read undefined symbols from libludork.so.")
        unresolved_ffmpeg = unresolved_ffmpeg_symbols(undefined.stdout)
        if unresolved_ffmpeg:
            raise PackError(
                "libludork.so has unresolved FFmpeg API symbols:\n"
                + "\n".join(unresolved_ffmpeg)
            )


def validate_unsigned_apk(
    context: PackContext,
    apk: pathlib.Path,
    manifest: RuntimeManifest,
) -> None:
    validate_apk_archive(apk, manifest)
    validate_apk_metadata(context, apk)
    validate_native_library(context, apk)


def validate_signed_apk(
    context: PackContext,
    apk: pathlib.Path,
    manifest: RuntimeManifest,
) -> None:
    validate_apk_archive(apk, manifest, signed=True)
    validate_apk_metadata(context, apk, signed=True)
    validate_native_library(context, apk)


def _published_apk_path(context: PackContext, signed: bool) -> pathlib.Path:
    qualifier = "signed" if signed else "unsigned"
    return (
        context.dist_dir
        / f"{context.artifact_name}-android-{ANDROID_ABI}-{qualifier}.apk"
    )


def _published_apk_temporary_path(
    context: PackContext,
    signed: bool,
) -> pathlib.Path:
    destination = _published_apk_path(context, signed)
    return destination.with_name(f".{destination.name}.tmp")


def _signed_apk_path(unsigned_apk: pathlib.Path) -> pathlib.Path:
    return unsigned_apk.with_name("app-release-signed.apk")


def cleanup_signing_outputs(
    context: PackContext,
    signed_apk: pathlib.Path,
) -> tuple[str, ...]:
    paths = (
        signed_apk,
        pathlib.Path(str(signed_apk) + ".idsig"),
        _published_apk_temporary_path(context, True),
    )
    failures: list[str] = []
    for path in paths:
        try:
            path.unlink(missing_ok=True)
        except OSError as exception:
            failures.append(f"{path}: {exception}")
    return tuple(failures)


def signing_cleanup_diagnostic(failures: tuple[str, ...]) -> str:
    if not failures:
        return ""
    return "Unable to remove incomplete Android signing output:\n" + "\n".join(failures)


def sign_apk(
    context: PackContext,
    unsigned_apk: pathlib.Path,
    signing: AndroidSigningOptions,
) -> pathlib.Path:
    signed_apk = _signed_apk_path(unsigned_apk)
    cleanup_diagnostic = signing_cleanup_diagnostic(
        cleanup_signing_outputs(context, signed_apk)
    )
    if cleanup_diagnostic:
        raise PackError(cleanup_diagnostic, EXIT_SIGNING)
    password_input = signing.keystore_password + "\n" + signing.key_password + "\n"
    result = _run_signing_tool(
        [
            str(context.sdk.apksigner),
            "sign",
            "--ks",
            str(signing.keystore_path),
            "--ks-key-alias",
            signing.key_alias,
            "--ks-pass",
            "stdin",
            "--key-pass",
            "stdin",
            "--min-sdk-version",
            str(ANDROID_MIN_SDK),
            "--v4-signing-enabled",
            "false",
            "--out",
            str(signed_apk),
            str(unsigned_apk),
        ],
        signing,
        input_text=password_input,
        environment=context.environment,
    )
    if result.returncode != 0:
        diagnostic = _signing_tool_diagnostic(result, signing)
        message = "Unable to sign the Android APK."
        if diagnostic:
            message += "\n" + diagnostic
        raise PackError(message, EXIT_SIGNING)
    pathlib.Path(str(signed_apk) + ".idsig").unlink(missing_ok=True)
    if not signed_apk.is_file():
        raise PackError(
            "apksigner did not produce the signed Android APK.",
            EXIT_SIGNING,
        )
    return signed_apk


def publish_apk(
    context: PackContext,
    apk: pathlib.Path,
    *,
    signed: bool = False,
) -> pathlib.Path:
    context.dist_dir.mkdir(parents=True, exist_ok=True)
    destination = _published_apk_path(context, signed)
    temporary = _published_apk_temporary_path(context, signed)
    if temporary.exists():
        temporary.unlink()
    try:
        shutil.copy2(apk, temporary)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            temporary.unlink()
    return destination


def sign_validate_and_publish_apk(
    context: PackContext,
    unsigned_apk: pathlib.Path,
    manifest: RuntimeManifest,
    signing: AndroidSigningOptions,
) -> pathlib.Path:
    signed_apk = _signed_apk_path(unsigned_apk)
    try:
        signed_apk = sign_apk(context, unsigned_apk, signing)
        validate_signed_apk(context, signed_apk, manifest)
        return publish_apk(context, signed_apk, signed=True)
    except PackError as exception:
        cleanup_diagnostic = signing_cleanup_diagnostic(
            cleanup_signing_outputs(context, signed_apk)
        )
        message = redact_signing_diagnostic(str(exception), signing)
        if cleanup_diagnostic and cleanup_diagnostic not in message:
            message += "\n" + cleanup_diagnostic
        raise PackError(
            message,
            EXIT_SIGNING,
        ) from exception
    except (OSError, RuntimeError, zipfile.BadZipFile) as exception:
        cleanup_diagnostic = signing_cleanup_diagnostic(
            cleanup_signing_outputs(context, signed_apk)
        )
        message = redact_signing_diagnostic(str(exception), signing)
        if cleanup_diagnostic and cleanup_diagnostic not in message:
            message += "\n" + cleanup_diagnostic
        raise PackError(message, EXIT_SIGNING) from exception


def validate_template_manifest(path: pathlib.Path) -> None:
    try:
        root = ElementTree.parse(path).getroot()
    except ElementTree.ParseError as exception:
        raise PackError(
            f"The Android manifest is invalid XML: {exception}",
            EXIT_PROJECT,
        ) from exception
    android_attribute = f"{{{ANDROID_XML_NAMESPACE}}}"
    application = root.find("application")
    if (
        application is None
        or application.get(android_attribute + "appCategory")
        != ANDROID_APP_CATEGORY
    ):
        raise PackError(
            'The Android manifest application must declare android:appCategory="game".',
            EXIT_PROJECT,
        )
    activity = next(
        (
            candidate
            for candidate in application.findall("activity")
            if candidate.get(android_attribute + "name") == ANDROID_ACTIVITY_NAME
        ),
        None,
    )
    if activity is None:
        raise PackError(
            f"The Android manifest does not declare {ANDROID_ACTIVITY_NAME}.",
            EXIT_PROJECT,
        )
    if (
        activity.get(android_attribute + "screenOrientation")
        != ANDROID_SCREEN_ORIENTATION
    ):
        raise PackError(
            "The Android launch activity must declare "
            'android:screenOrientation="sensorLandscape".',
            EXIT_PROJECT,
        )


def validate_template_source(context: PackContext) -> None:
    required = (
        context.template_dir / "settings.gradle.kts",
        context.template_dir / "build.gradle.kts",
        context.template_dir / "gradle.properties",
        context.template_dir / "gradle" / "wrapper" / "gradle-wrapper.properties",
        context.template_dir / "app" / "build.gradle.kts",
        context.template_dir / "app" / "src" / "main" / "AndroidManifest.xml",
        context.template_dir
        / "app"
        / "src"
        / "main"
        / "java"
        / "com"
        / "ludork"
        / "android"
        / "LudorkActivity.java",
    )
    wrapper_root = (
        context.project_dir
        / "LuaSF"
        / "third_party"
        / "SFML"
        / "examples"
        / "projects"
        / "android"
    )
    required += (
        wrapper_root / "gradlew",
        wrapper_root / "gradlew.bat",
        wrapper_root / "gradle" / "wrapper" / "gradle-wrapper.jar",
    )
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise PackError(
            "Android Gradle template or wrapper files are incomplete:\n" + "\n".join(missing),
            EXIT_PROJECT,
        )
    validate_template_manifest(
        context.template_dir / "app" / "src" / "main" / "AndroidManifest.xml"
    )
    root_build = (context.template_dir / "build.gradle.kts").read_text(encoding="utf-8")
    app_build = (context.template_dir / "app" / "build.gradle.kts").read_text(encoding="utf-8")
    wrapper = (
        context.template_dir / "gradle" / "wrapper" / "gradle-wrapper.properties"
    ).read_text(encoding="utf-8")
    expected_fragments = (
        (root_build, f'version "{ANDROID_GRADLE_PLUGIN_VERSION}"'),
        (wrapper, f"gradle-{GRADLE_VERSION}-bin.zip"),
        (app_build, f"compileSdk = {ANDROID_COMPILE_SDK}"),
        (app_build, f"minSdk = {ANDROID_MIN_SDK}"),
        (app_build, f"targetSdk = {ANDROID_TARGET_SDK}"),
        (app_build, f'buildToolsVersion = "{ANDROID_BUILD_TOOLS}"'),
    )
    if any(fragment not in text for text, fragment in expected_fragments):
        raise PackError("Android Gradle template versions do not match the packaging contract.", EXIT_PROJECT)
    template_text = "\n".join(
        path.read_text(encoding="utf-8")
        for path in required[:7]
        if path.suffix != ".jar"
    )
    forbidden = ("sdk.dir=", "ndk.dir=", "cmake.dir=", "externalNativeBuild")
    if any(value in template_text for value in forbidden):
        raise PackError("Android Gradle templates must not carry SDK, NDK, or native build paths.", EXIT_PROJECT)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="ScriptTools android-pack")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--compile-lua", action="store_true")
    parser.add_argument("--encrypt-shaders", action="store_true")
    parser.add_argument("--encrypt-data", action="store_true")
    parser.add_argument("--sign", action="store_true")
    parser.add_argument("--keystore", type=pathlib.Path)
    parser.add_argument("--key-alias")
    parser.add_argument("project_folder", type=pathlib.Path)
    parser.add_argument("dist_folder", type=pathlib.Path, nargs="?")
    return parser


def main(arguments: list[str] | None = None) -> int:
    parser = create_parser()
    parsed = parser.parse_args(arguments)
    signing: AndroidSigningOptions | None = None
    try:
        signing = read_signing_options(parsed, sys.stdin)
        context = create_context(parsed)
        validate_template_source(context)
        print(f"Android Studio: {context.studio.app}")
        print(f"Android Studio JBR: {context.studio.java_home}")
        print(f"Android SDK: {context.sdk.root}")
        print(f"Android NDK: {context.ndk.root} ({context.ndk.revision})")
        cmake_version = ".".join(str(value) for value in context.cmake.version)
        print(f"System CMake: {context.cmake.executable} ({cmake_version})")
        print(f"Unix Makefiles make: {context.make}")
        print(f"Application ID: {context.application_id}")
        if signing is not None:
            validate_signing_credentials(context, signing)
        if parsed.check:
            packaging_kind = "signed" if signing is not None else "unsigned"
            print(f"Android {packaging_kind} APK packaging check passed.")
            return 0
        copy_runtime_resources(context)
        manifest = create_runtime_manifest(context.runtime_dir)
        prepare_gradle_stage(context, manifest)
        print(f"Runtime SHA-256: {manifest.digest}")
        build_native_library(context, manifest.digest)
        apk = build_unsigned_apk(context)
        validate_unsigned_apk(context, apk, manifest)
        output = (
            publish_apk(context, apk)
            if signing is None
            else sign_validate_and_publish_apk(context, apk, manifest, signing)
        )
        print(f"Pack complete: {output}")
        return 0
    except PackError as exception:
        message = str(exception)
        if signing is not None:
            message = redact_signing_diagnostic(message, signing)
        print(message, file=sys.stderr)
        return exception.exit_code
    except (OSError, RuntimeError, zipfile.BadZipFile) as exception:
        message = str(exception)
        if signing is not None:
            message = redact_signing_diagnostic(message, signing)
        print(message, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
