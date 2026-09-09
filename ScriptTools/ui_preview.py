from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections.abc import Callable, Iterator
from contextlib import contextmanager
from dataclasses import dataclass

from .ui_control_registry import (
    UiControlRegistry,
    UiRegistryError,
    load_registry,
    read_registry,
    require_fields,
    strict_json,
)
from .ui_property_values import UiAssetError


PREVIEW_DIRECTORY = pathlib.Path("Binaries")
METADATA_DIRECTORY = pathlib.Path("Temp")
MANIFEST_NAME = "UiPreview.json"
REGISTRY_NAME = "UiPreview.registry.json"
BUILDING_NAME = "UiPreview.building"
MANIFEST_FIELDS = {
    "formatVersion",
    "buildId",
    "platform",
    "architecture",
    "configuration",
    "runtimeDirectory",
    "files",
    "registryHash",
    "adapterFingerprint",
}
BUILD_INFO_FIELDS = {
    "formatVersion",
    "platform",
    "architecture",
    "configuration",
    "files",
}


def is_preview_development_file(name: str) -> bool:
    return (
        name
        in {
            "UiPreviewHost",
            "UiPreviewHost.exe",
            MANIFEST_NAME,
            REGISTRY_NAME,
            "UiPreviewHostRuntime.dll",
        }
        or re.fullmatch(
            r"libUiPreviewHostRuntime(?:\.\d+)*\.dylib|libUiPreviewHostRuntime\.so(?:\.\d+)*",
            name,
        )
        is not None
    )


def _runtime_files(value: object) -> list[str]:
    if not isinstance(value, list) or not value:
        raise UiRegistryError("Preview files must be a non-empty array")
    seen: set[str] = set()
    for name in value:
        if (
            not isinstance(name, str)
            or not name
            or name in (".", "..")
            or name != name.strip()
            or name.endswith(".")
            or any(c in name for c in '/\\:\0<>"|?*')
            or any(ord(c) < 32 or 127 <= ord(c) <= 159 for c in name)
            or name.casefold() in seen
            or name.casefold() in (MANIFEST_NAME.casefold(), REGISTRY_NAME.casefold())
        ):
            raise UiRegistryError(f"Invalid or duplicate preview filename: {name!r}")
        stem = name.split(".")[0].upper()
        if stem in ("CON", "PRN", "AUX", "NUL", "CLOCK$") or re.fullmatch(
            r"(?:COM|LPT)[1-9¹²³]", stem
        ):
            raise UiRegistryError(f"Reserved preview filename: {name}")
        seen.add(name.casefold())
    return sorted(value, key=lambda name: name.encode("utf-8"))


def _is_link(path: pathlib.Path) -> bool:
    return path.is_symlink() or bool(getattr(path, "is_junction", lambda: False)())


def _artifact_root(
    project: pathlib.Path, directory: pathlib.Path = PREVIEW_DIRECTORY
) -> pathlib.Path:
    root = pathlib.Path(project).expanduser().resolve()
    current = root
    for name in directory.parts:
        current /= name
        if _is_link(current):
            raise UiRegistryError(f"Preview output must not be a link: {current}")
    return current


def _runtime_directory(project: pathlib.Path, value: object) -> pathlib.Path:
    if not isinstance(value, str) or not value or "\\" in value:
        raise UiRegistryError("Preview runtimeDirectory must be a relative directory")
    parts = value.split("/")
    if parts[0].casefold() == METADATA_DIRECTORY.as_posix().casefold():
        raise UiRegistryError("Preview binaries must not be inside project Temp")
    for part in parts:
        _runtime_files([part])
    return _artifact_root(project, pathlib.Path(*parts))


def _relative_runtime_directory(project: pathlib.Path, directory: pathlib.Path) -> str:
    try:
        relative = directory.absolute().relative_to(project.resolve()).as_posix()
    except ValueError as error:
        raise UiRegistryError("Preview runtime must be inside its project") from error
    if _runtime_directory(project, relative).resolve() != directory.resolve():
        raise UiRegistryError("Invalid preview runtime directory")
    return relative


def _require_not_building(project: pathlib.Path) -> None:
    marker = _artifact_root(project, METADATA_DIRECTORY) / BUILDING_NAME
    if os.path.lexists(marker):
        raise UiRegistryError(
            "Project preview build has not completed; rebuild the C++ project"
        )


@contextmanager
def _metadata_lock(project: pathlib.Path) -> Iterator[None]:
    identity = os.path.normcase(str(project.expanduser().resolve())).encode("utf-8")
    lock_root = pathlib.Path(tempfile.gettempdir()) / "LudorkUiPreviewLocks"
    if _is_link(lock_root):
        raise UiRegistryError(f"Preview lock directory must not be a link: {lock_root}")
    lock_root.mkdir(exist_ok=True)
    path = lock_root / (hashlib.sha256(identity).hexdigest() + ".lock")
    if _is_link(path):
        raise UiRegistryError(f"Preview lock must not be a link: {path}")
    with path.open("a+b") as lock:
        if lock.tell() == 0:
            lock.write(b"\0")
            lock.flush()
        deadline = time.monotonic() + 30
        while True:
            try:
                lock.seek(0)
                if os.name == "nt":
                    import msvcrt

                    msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl

                    fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except (BlockingIOError, PermissionError):
                if time.monotonic() >= deadline:
                    raise UiRegistryError(
                        "Timed out waiting for another preview metadata operation"
                    )
                time.sleep(0.1)
        try:
            yield
        finally:
            lock.seek(0)
            if os.name == "nt":
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def _architecture() -> str:
    machine = platform.machine().lower()
    return {"amd64": "x64", "x86_64": "x64", "aarch64": "arm64"}.get(machine, machine)


def _host_name(target_platform: str) -> str:
    return "UiPreviewHost.exe" if target_platform == "win32" else "UiPreviewHost"


def _sha256_file(path: pathlib.Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def _check_relative_link(path: pathlib.Path, root: pathlib.Path) -> str:
    target = os.readlink(path)
    normalized = target.replace("\\", "/")
    if (
        ":" in normalized
        or len(pathlib.PurePosixPath(normalized).parts) != 1
        or ".." in normalized.split("/")
    ):
        raise UiRegistryError(
            f"Preview library link must stay in its directory: {path}"
        )
    if (
        pathlib.Path(target).is_absolute()
        or pathlib.PureWindowsPath(target).is_absolute()
    ):
        raise UiRegistryError(f"Preview library link must be relative: {path}")
    try:
        path.resolve(strict=True).relative_to(root.resolve())
    except (OSError, ValueError, RuntimeError) as error:
        raise UiRegistryError(
            f"Preview library link leaves the runtime directory: {path}"
        ) from error
    if not path.is_file():
        raise UiRegistryError(f"Preview library link must target a file: {path}")
    return normalized


def runtime_digest(
    directory: pathlib.Path, files: list[str], registry_file: pathlib.Path
) -> str:
    digest = hashlib.sha256()
    names = _runtime_files(files)
    if _is_link(registry_file) or not registry_file.is_file():
        raise UiRegistryError(
            f"Preview registry must be a regular file: {registry_file}"
        )
    for name in sorted(names + [REGISTRY_NAME], key=lambda name: name.encode("utf-8")):
        path = registry_file if name == REGISTRY_NAME else directory / name
        digest.update(name.encode("utf-8") + b"\0")
        if path.is_symlink():
            target = _check_relative_link(path, directory)
            if (
                pathlib.PurePosixPath(target).name not in names
                or path.resolve().name not in names
            ):
                raise UiRegistryError(f"Preview link target is not listed: {path}")
            digest.update(b"L\0" + target.encode("utf-8") + b"\0")
        elif path.is_file() and not _is_link(path):
            digest.update(b"F\0" + _sha256_file(path).encode("ascii") + b"\0")
        else:
            raise UiRegistryError(f"Unexpected preview runtime entry: {path}")
    return digest.hexdigest()


@dataclass(frozen=True)
class UiPreviewSnapshot:
    root: pathlib.Path
    runtime_directory: pathlib.Path
    host_path: pathlib.Path
    registry_path: pathlib.Path
    registry: UiControlRegistry
    manifest: dict[str, object]


def load_preview(
    project: pathlib.Path, *, require_host_platform: bool = True
) -> UiPreviewSnapshot:
    _require_not_building(project)
    root = _artifact_root(project, METADATA_DIRECTORY)
    current = root / MANIFEST_NAME
    try:
        if _is_link(current):
            raise UiRegistryError(f"Preview manifest must not be a link: {current}")
        manifest = require_fields(
            strict_json(current.read_bytes()), MANIFEST_FIELDS, "Preview manifest"
        )
    except OSError as error:
        raise UiRegistryError(
            f"Project preview is unavailable: {current}. Build the C++ project or update the Standalone preview."
        ) from error
    return _read_snapshot(
        project, manifest, require_host_platform=require_host_platform
    )


def _read_snapshot(
    project: pathlib.Path,
    manifest: dict[str, object],
    *,
    require_host_platform: bool = True,
) -> UiPreviewSnapshot:
    root = _artifact_root(project, METADATA_DIRECTORY)
    if type(manifest["formatVersion"]) is not int or manifest["formatVersion"] != 3:
        raise UiRegistryError(
            "Unsupported preview manifest format; rebuild the project preview"
        )
    for name in ("buildId", "registryHash", "adapterFingerprint"):
        value = manifest[name]
        if not isinstance(value, str) or re.fullmatch(r"[0-9a-f]{64}", value) is None:
            raise UiRegistryError(f"Invalid preview {name}")
    files = _runtime_files(manifest["files"])
    if manifest["configuration"] not in ("Debug", "Release"):
        raise UiRegistryError("Invalid preview configuration")
    if manifest["platform"] not in ("win32", "darwin", "linux") or manifest[
        "architecture"
    ] not in ("x64", "arm64", "x86", "arm"):
        raise UiRegistryError("Invalid preview platform or architecture")
    if require_host_platform and (
        manifest["platform"] != sys.platform
        or manifest["architecture"] != _architecture()
    ):
        raise UiRegistryError(
            "The project preview was built for another platform or architecture; rebuild it on this machine"
        )
    directory = _runtime_directory(project, manifest["runtimeDirectory"])
    if not directory.is_dir():
        raise UiRegistryError(f"Preview runtime is missing: {directory}")
    if _host_name(manifest["platform"]) not in files:
        raise UiRegistryError("Preview files must include the Host")
    registry_file = root / REGISTRY_NAME
    if _is_link(registry_file):
        raise UiRegistryError("Preview registry must not be a link")
    registry = load_registry(registry_file)
    if (
        registry.digest != manifest["registryHash"]
        or registry.fingerprint != manifest["adapterFingerprint"]
    ):
        raise UiRegistryError(
            "Preview manifest and registry do not match; rebuild the project preview"
        )
    host = directory / _host_name(manifest["platform"])
    if _is_link(host) or not host.is_file():
        raise UiRegistryError(f"Preview Host is missing: {host}")
    if runtime_digest(directory, files, registry_file) != manifest["buildId"]:
        raise UiRegistryError(
            "Preview runtime files do not match their immutable build; rebuild the project preview"
        )
    return UiPreviewSnapshot(root, directory, host, registry_file, registry, manifest)


def registry_path(project: pathlib.Path) -> pathlib.Path:
    return ensure_preview(project).registry_path


def prepare_registry(project: pathlib.Path, script_tools: pathlib.Path) -> pathlib.Path:
    project = project.expanduser().resolve()
    script_tools = script_tools.expanduser().resolve()
    name = (
        "build_ui_preview_host.bat" if os.name == "nt" else "build_ui_preview_host.sh"
    )
    candidates = (
        script_tools.parent / name,
        script_tools.parent.parent.parent / "tools" / name,
    )
    script = next((path for path in candidates if path.is_file()), None)
    if script is None:
        raise UiRegistryError(
            f"Project preview build tool was not found beside {script_tools}"
        )
    environment = os.environ.copy()
    if os.name == "nt":
        environment["LUDORK_UI_PREVIEW_BUILD_SCRIPT"] = str(script)
        environment["LUDORK_UI_PREVIEW_PROJECT"] = str(project)
        command = (
            subprocess.list2cmdline([environment.get("COMSPEC", "cmd.exe")])
            + ' /d /s /c ""%LUDORK_UI_PREVIEW_BUILD_SCRIPT%" "%LUDORK_UI_PREVIEW_PROJECT%" Release"'
        )
    else:
        command = ["sh", str(script), str(project), "Release"]
    result = subprocess.run(command, cwd=project, env=environment, check=False)
    if result.returncode:
        raise UiRegistryError(
            f"Project preview build failed with exit code {result.returncode}: {project}"
        )
    return registry_path(project)


def describe_host(host: pathlib.Path, project: pathlib.Path) -> UiControlRegistry:
    return read_registry(_query_host(host, project, "--describe"))


def _query_host(host: pathlib.Path, project: pathlib.Path, option: str) -> bytes:
    if _is_link(host) or not host.is_file():
        raise UiRegistryError(
            f"Preview Host is missing or is not a regular file: {host}"
        )
    try:
        result = subprocess.run(
            [str(host), option],
            cwd=project,
            capture_output=True,
            timeout=30,
            check=False,
            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise UiRegistryError(
            f"Unable to describe project preview: {host}: {error}"
        ) from error
    if result.returncode:
        raise UiRegistryError(
            f"Project preview description failed ({result.returncode}): {result.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return result.stdout


def _build_info(host: pathlib.Path, project: pathlib.Path) -> dict[str, object]:
    info = require_fields(
        strict_json(_query_host(host, project, "--build-info")),
        BUILD_INFO_FIELDS,
        "Preview build info",
    )
    if type(info["formatVersion"]) is not int or info["formatVersion"] != 1:
        raise UiRegistryError(
            "Unsupported preview build info; update the project preview"
        )
    if info["platform"] != sys.platform or info["architecture"] != _architecture():
        raise UiRegistryError(
            "Preview build info does not match this platform or architecture"
        )
    if info["configuration"] not in ("Debug", "Release"):
        raise UiRegistryError("Invalid preview build configuration")
    info["files"] = _runtime_files(info["files"])
    if host.name not in info["files"]:
        raise UiRegistryError("Preview build info must include the Host")
    return info


def _retry_file_operation(operation: Callable[[], object]) -> None:
    deadline = time.monotonic() + 10
    delay = 0.05
    while True:
        try:
            operation()
            return
        except OSError as error:
            if (
                os.name != "nt"
                or (
                    not isinstance(error, PermissionError)
                    and getattr(error, "winerror", None) not in (5, 32, 33)
                )
                or time.monotonic() >= deadline
            ):
                raise
            time.sleep(min(delay, max(0, deadline - time.monotonic())))
            delay = min(delay * 2, 0.4)


def _replace(source: pathlib.Path, destination: pathlib.Path) -> None:
    _retry_file_operation(lambda: source.replace(destination))


def _wait_for_runtime_release(
    directory: pathlib.Path, files: list[pathlib.Path] | None = None
) -> None:
    if os.name != "nt":
        return

    def check_files() -> None:
        for path in files if files is not None else directory.rglob("*"):
            if path.is_file() and not _is_link(path):
                # A mapped DLL can survive a directory rename but cannot be opened for writing.
                with path.open("r+b"):
                    pass

    _retry_file_operation(check_files)


def _temporary_root(project: pathlib.Path) -> pathlib.Path:
    root = _artifact_root(project, METADATA_DIRECTORY)
    root.mkdir(parents=True, exist_ok=True)
    return root


def _remove_legacy_output(project: pathlib.Path) -> None:
    tools = project / "Tools"
    legacy = tools / "UiPreview"
    if not os.path.lexists(legacy):
        return
    if _is_link(tools) or _is_link(legacy) or not legacy.is_dir():
        raise UiRegistryError(
            f"Obsolete preview output is not a regular directory: {legacy}"
        )
    shutil.rmtree(legacy)
    if tools.is_dir() and not any(tools.iterdir()):
        tools.rmdir()


def _copy_entry(source: pathlib.Path, destination: pathlib.Path) -> None:
    if os.path.lexists(destination):
        if destination.is_dir() and not destination.is_symlink():
            raise UiRegistryError(
                f"Preview file conflicts with a directory: {destination}"
            )
        destination.unlink()
    if source.is_symlink():
        destination.symlink_to(os.readlink(source))
    elif source.is_file() and not _is_link(source):
        shutil.copy2(source, destination)
    else:
        raise UiRegistryError(f"Unexpected preview runtime dependency: {source}")


def _remove_obsolete_project_binaries(
    project: pathlib.Path, snapshot: UiPreviewSnapshot
) -> None:
    project_file = project / "Main.proj"
    if not project_file.is_file():
        return
    config = strict_json(project_file.read_bytes())
    if not isinstance(config, dict) or config.get("Cpp") is not True:
        return
    old_root = _artifact_root(project)
    if (
        old_root == snapshot.runtime_directory
        or not (old_root / _host_name(sys.platform)).is_file()
    ):
        return
    removable = []
    for name in snapshot.manifest["files"]:
        old = old_root / name
        if not os.path.lexists(old):
            continue
        if is_preview_development_file(name) or _entry_identity(old) == _entry_identity(
            snapshot.runtime_directory / name
        ):
            removable.append(old)
    _wait_for_runtime_release(old_root, removable)
    for path in removable:
        path.unlink()
    if not any(old_root.iterdir()):
        old_root.rmdir()


def _entry_identity(path: pathlib.Path) -> tuple[str, str]:
    if path.is_symlink():
        return ("L", os.readlink(path).replace("\\", "/"))
    if path.is_file() and not _is_link(path):
        return ("F", _sha256_file(path))
    raise UiRegistryError(f"Unexpected runtime dependency: {path}")


def _install_snapshot(
    project: pathlib.Path,
    source_directory: pathlib.Path,
    registry: UiControlRegistry,
    manifest: dict[str, object],
    *,
    shared_game_files: bool,
) -> UiPreviewSnapshot:
    root = _runtime_directory(project, manifest["runtimeDirectory"])
    copy_binaries = root.resolve() != source_directory.resolve()
    metadata_root = _artifact_root(project, METADATA_DIRECTORY)
    files = _runtime_files(manifest["files"])
    data = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    current = metadata_root / MANIFEST_NAME
    for name in (REGISTRY_NAME, MANIFEST_NAME):
        path = metadata_root / name
        if os.path.lexists(path) and (_is_link(path) or not path.is_file()):
            raise UiRegistryError(f"Preview metadata must be a regular file: {path}")
    if current.is_file() and current.read_bytes() == data:
        try:
            existing = _read_snapshot(project, manifest)
        except (UiRegistryError, OSError):
            if shared_game_files:
                raise
        else:
            _remove_legacy_output(project)
            return existing
    transaction = pathlib.Path(
        tempfile.mkdtemp(prefix="ui-preview-publish-", dir=_temporary_root(project))
    )
    staging = transaction / "runtime" if copy_binaries else root
    backup = transaction / "previous"
    preserve_transaction = False
    installed: list[pathlib.Path] = []
    backups: list[tuple[pathlib.Path, pathlib.Path]] = []
    try:
        if copy_binaries and root.exists():
            if not root.is_dir():
                raise UiRegistryError(f"Binaries is not a directory: {root}")
            for path in root.rglob("*"):
                if bool(getattr(path, "is_junction", lambda: False)()):
                    raise UiRegistryError(f"Binaries contains a junction: {path}")
            shutil.copytree(root, staging, symlinks=True)
        elif copy_binaries:
            staging.mkdir()
        for name in (REGISTRY_NAME, MANIFEST_NAME) if copy_binaries else ():
            obsolete = staging / name
            if os.path.lexists(obsolete):
                if _is_link(obsolete) or not obsolete.is_file():
                    raise UiRegistryError(
                        f"Obsolete preview metadata is not a regular file: {obsolete}"
                    )
                obsolete.unlink()
        for name in files if copy_binaries else ():
            source = source_directory / name
            destination = staging / name
            if source.is_symlink():
                _check_relative_link(source, source_directory)
            if os.path.lexists(destination):
                same = _entry_identity(source) == _entry_identity(destination)
                if same:
                    continue
                if shared_game_files and not is_preview_development_file(name):
                    raise UiRegistryError(
                        f"Game and preview dependency do not match: {name}. Build them together."
                    )
            _copy_entry(source, destination)
        registry_file = transaction / REGISTRY_NAME
        registry_file.write_bytes(registry.raw)
        if runtime_digest(staging, files, registry_file) != manifest["buildId"]:
            raise UiRegistryError("Preview files changed while preparing Binaries")
        if (
            describe_host(staging / _host_name(manifest["platform"]), project).raw
            != registry.raw
        ):
            raise UiRegistryError(
                "Shared preview runtime does not describe the same registry"
            )
        manifest_file = transaction / MANIFEST_NAME
        with manifest_file.open("wb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        metadata_root.mkdir(parents=True, exist_ok=True)
        try:
            if current.exists():
                previous = transaction / ("previous-" + MANIFEST_NAME)
                _replace(current, previous)
                backups.append((previous, current))
            if copy_binaries and root.exists():
                _replace(root, backup)
                backups.append((backup, root))
                _wait_for_runtime_release(backup)
            if copy_binaries:
                root.parent.mkdir(parents=True, exist_ok=True)
                _replace(staging, root)
                installed.append(root)
            destination = metadata_root / REGISTRY_NAME
            if destination.exists():
                previous = transaction / ("previous-" + REGISTRY_NAME)
                _replace(destination, previous)
                backups.append((previous, destination))
            _replace(registry_file, destination)
            installed.append(destination)
            result = _read_snapshot(project, manifest)
            _replace(manifest_file, current)
            installed.append(current)
        except (OSError, UiRegistryError):
            try:
                for destination in reversed(installed):
                    if destination == root:
                        shutil.rmtree(destination)
                    else:
                        destination.unlink()
                for previous, destination in reversed(backups):
                    _replace(previous, destination)
            except OSError as rollback_error:
                preserve_transaction = True
                raise UiRegistryError(
                    f"Preview installation rollback failed; original files are at {transaction}"
                ) from rollback_error
            raise
    finally:
        if not preserve_transaction:
            shutil.rmtree(transaction)
    _remove_legacy_output(project)
    return result


def publish_preview(
    project: pathlib.Path,
    runtime_directory: pathlib.Path,
    configuration: str,
) -> UiPreviewSnapshot:
    if configuration not in ("Debug", "Release"):
        raise UiRegistryError("Invalid preview configuration")
    project = project.expanduser().resolve()
    runtime_directory = runtime_directory.expanduser().absolute()
    with _metadata_lock(project):
        marker = _artifact_root(project, METADATA_DIRECTORY) / BUILDING_NAME
        if os.path.lexists(marker):
            if _is_link(marker) or not marker.is_file():
                raise UiRegistryError(
                    f"Preview build marker is not a regular file: {marker}"
                )
            if strict_json(marker.read_bytes()) != {
                "configuration": configuration,
                "runtimeDirectory": _relative_runtime_directory(
                    project, runtime_directory
                ),
            }:
                raise UiRegistryError(
                    "Another preview build configuration is in progress"
                )
        snapshot = _describe_snapshot(project, runtime_directory, configuration)
        result = _install_snapshot(
            project,
            runtime_directory,
            snapshot.registry,
            snapshot.manifest,
            shared_game_files=False,
        )
        _remove_obsolete_project_binaries(project, result)
        if os.path.lexists(marker):
            if _is_link(marker) or not marker.is_file():
                raise UiRegistryError(
                    f"Preview build marker is not a regular file: {marker}"
                )
            marker.unlink()
        return result


def _describe_snapshot(
    project: pathlib.Path,
    runtime_directory: pathlib.Path,
    configuration: str | None = None,
) -> UiPreviewSnapshot:
    relative = _relative_runtime_directory(project, runtime_directory)
    host = runtime_directory / _host_name(sys.platform)
    info = _build_info(host, project)
    if configuration is not None and info["configuration"] != configuration:
        raise UiRegistryError(
            "The compiled preview configuration does not match the requested build"
        )
    registry = describe_host(host, project)
    temporary = pathlib.Path(
        tempfile.mkdtemp(prefix="ui-preview-describe-", dir=_temporary_root(project))
    )
    try:
        registry_file = temporary / REGISTRY_NAME
        registry_file.write_bytes(registry.raw)
        manifest = {
            "formatVersion": 3,
            "buildId": runtime_digest(runtime_directory, info["files"], registry_file),
            "platform": info["platform"],
            "architecture": info["architecture"],
            "configuration": info["configuration"],
            "runtimeDirectory": relative,
            "registryHash": registry.digest,
            "adapterFingerprint": registry.fingerprint,
            "files": info["files"],
        }
        root = _artifact_root(project, METADATA_DIRECTORY)
        return UiPreviewSnapshot(
            root, runtime_directory, host, root / REGISTRY_NAME, registry, manifest
        )
    finally:
        shutil.rmtree(temporary)


def copy_preview(
    source: pathlib.Path, destination: pathlib.Path, runtime_directory: str = "Binaries"
) -> UiPreviewSnapshot:
    snapshot = load_preview(source)
    destination = destination.expanduser().resolve()
    _runtime_directory(destination, runtime_directory)
    with _metadata_lock(destination):
        _require_not_building(destination)
        return _install_snapshot(
            destination,
            snapshot.runtime_directory,
            snapshot.registry,
            dict(snapshot.manifest, runtimeDirectory=runtime_directory),
            shared_game_files=True,
        )


def begin_build(
    project: pathlib.Path, runtime_directory: pathlib.Path, configuration: str
) -> None:
    project = project.expanduser().resolve()
    runtime_directory = runtime_directory.expanduser().absolute()
    relative = _relative_runtime_directory(project, runtime_directory)
    with _metadata_lock(project):
        root = _temporary_root(project)
        marker = root / BUILDING_NAME
        if os.path.lexists(marker) and (_is_link(marker) or not marker.is_file()):
            raise UiRegistryError(
                f"Preview build marker is not a regular file: {marker}"
            )
        marker.write_text(
            json.dumps({"configuration": configuration, "runtimeDirectory": relative}),
            encoding="utf-8",
        )
        current = root / MANIFEST_NAME
        if os.path.lexists(current):
            if _is_link(current) or not current.is_file():
                raise UiRegistryError(
                    f"Preview manifest must be a regular file: {current}"
                )
        _wait_for_runtime_release(runtime_directory)


def ensure_preview(project: pathlib.Path) -> UiPreviewSnapshot:
    project = project.expanduser().resolve()
    _require_not_building(project)
    root = _artifact_root(project, METADATA_DIRECTORY)
    paths = [root / MANIFEST_NAME, root / REGISTRY_NAME]
    if all(os.path.lexists(path) for path in paths):
        return load_preview(project)
    project_file = project / "Main.proj"
    config = strict_json(project_file.read_bytes()) if project_file.is_file() else None
    if not isinstance(config, dict) or config.get("Cpp") is not False:
        return load_preview(project)
    with _metadata_lock(project):
        _require_not_building(project)
        if all(os.path.lexists(path) for path in paths):
            return load_preview(project)
        for path in paths:
            if os.path.lexists(path) and (_is_link(path) or not path.is_file()):
                raise UiRegistryError(
                    f"Preview metadata must be a regular file: {path}"
                )
        snapshot = _describe_snapshot(project, _artifact_root(project))
        if paths[0].exists():
            existing = require_fields(
                strict_json(paths[0].read_bytes()), MANIFEST_FIELDS, "Preview manifest"
            )
            if (
                type(existing["formatVersion"]) is not int
                or existing["formatVersion"] != 3
            ):
                raise UiRegistryError(
                    "Unsupported preview manifest format; update the project preview"
                )
            if existing != snapshot.manifest:
                raise UiRegistryError(
                    "The remaining preview manifest does not match the compiled Host"
                )
        if paths[1].exists() and load_registry(paths[1]).raw != snapshot.registry.raw:
            raise UiRegistryError(
                "The remaining preview registry does not match the compiled Host"
            )
        return _install_snapshot(
            project,
            snapshot.runtime_directory,
            snapshot.registry,
            snapshot.manifest,
            shared_game_files=False,
        )


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools ui-preview")
    commands = parser.add_subparsers(dest="operation", required=True)
    for operation in ("publish", "begin-build"):
        command = commands.add_parser(operation)
        command.add_argument("project_root", type=pathlib.Path)
        command.add_argument("runtime_directory", type=pathlib.Path)
        command.add_argument(
            "--configuration", choices=("Debug", "Release"), required=True
        )
    copy = commands.add_parser("copy")
    copy.add_argument("project_root", type=pathlib.Path)
    copy.add_argument("destination", type=pathlib.Path)
    copy.add_argument("--runtime-directory", default="Binaries")
    for operation in ("validate", "registry", "ensure"):
        command = commands.add_parser(operation)
        command.add_argument("project_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        if parsed.operation == "begin-build":
            begin_build(
                parsed.project_root, parsed.runtime_directory, parsed.configuration
            )
            print("Project preview is paused for native compilation")
            return 0
        if parsed.operation == "publish":
            snapshot = publish_preview(
                parsed.project_root,
                parsed.runtime_directory,
                parsed.configuration,
            )
        elif parsed.operation == "copy":
            snapshot = copy_preview(
                parsed.project_root, parsed.destination, parsed.runtime_directory
            )
        elif parsed.operation in ("ensure", "registry"):
            snapshot = ensure_preview(parsed.project_root)
        else:
            snapshot = load_preview(parsed.project_root)
        if parsed.operation == "registry":
            print(snapshot.registry_path)
        else:
            print(
                f"Project preview {parsed.operation} completed: {snapshot.manifest['buildId']}"
            )
    except (UiAssetError, OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    return 0
