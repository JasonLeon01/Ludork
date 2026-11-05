from __future__ import annotations

import hashlib
import importlib.metadata
import json
import pathlib
import platform
import ssl
import shutil
import stat
import subprocess
import sys


PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from Toolchain.file_replace import remove_file, remove_tree, retry_file_operation
from Toolchain.cli.catalogue import COMMANDS
from Toolchain.runtime_bundle import (
    BUILD_MODE,
    BUNDLE_LAYOUT,
    SOURCE_STAMP_NAME,
    VERSION_REPORT_NAME,
    smoke_bundle,
    validate_bundle,
    write_manifest,
)

from Toolchain.tool_paths import TOOL_ROLES, executable_name


def _versions() -> dict[str, str]:
    return {
        "Build mode": BUILD_MODE,
        "Bundle layout": BUNDLE_LAYOUT,
        "Platform": f"{platform.system()}-{platform.machine()}",
        "CPython": platform.python_version(),
        "OpenSSL": ssl.OPENSSL_VERSION,
        "Nuitka": importlib.metadata.version("Nuitka"),
        "Pillow": importlib.metadata.version("Pillow"),
    }


def _source_hash(project: pathlib.Path, versions: dict[str, str]) -> str:
    paths = sorted((project / "Toolchain").rglob("*.py"))
    paths.extend(sorted((project / "tools/toolchain_entrypoints").glob("*.py")))
    paths.extend(project / relative for relative in (
        "requirements.txt", "tools/build_toolchain_bundle.py",
        "tools/build_toolchain_bundle.sh", "tools/build_toolchain_bundle.bat",
    ))
    digest = hashlib.sha256(json.dumps(versions, sort_keys=True).encode("utf-8"))
    for path in paths:
        digest.update(path.relative_to(project).as_posix().encode("utf-8") + b"\0")
        digest.update(path.read_bytes() + b"\0")
    return digest.hexdigest()


def _remove_owned(path: pathlib.Path) -> None:
    try:
        if path.is_symlink() or path.is_file():
            remove_file(path, missing_ok=True)
        elif path.is_dir():
            remove_tree(path)
    except OSError as error:
        raise RuntimeError(f"Toolchain cleanup failed for {path}: {error}") from error


def _rename_runtime(source: pathlib.Path, destination: pathlib.Path, stage: str) -> None:
    try:
        retry_file_operation(lambda: source.rename(destination))
    except OSError as error:
        raise RuntimeError(f"Toolchain {stage} failed: {source} -> {destination}: {error}") from error


def _distribution_entry(path: pathlib.Path, root: pathlib.Path) -> tuple[object, ...]:
    mode = stat.S_IMODE(path.lstat().st_mode)
    if path.is_symlink():
        target = path.readlink()
        if target.is_absolute() or not path.resolve().is_relative_to(root.resolve()) or not path.exists():
            raise RuntimeError(f"Distribution link leaves its directory or is broken: {path}")
        return ("link", mode, target.as_posix())
    if path.is_dir():
        return ("directory", mode)
    if path.is_file():
        with path.open("rb") as stream:
            return ("file", mode, path.stat().st_size, hashlib.file_digest(stream, "sha256").hexdigest())
    raise RuntimeError(f"Unsupported distribution entry: {path}")


def _merge_distribution(source: pathlib.Path, destination: pathlib.Path, role: str,
                        entries: dict[str, tuple[str, tuple[object, ...]]]) -> None:
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        key = relative.as_posix()
        state = _distribution_entry(path, source)
        previous = entries.get(key)
        if previous is not None:
            if previous[1] != state:
                raise RuntimeError(f"Toolchain dependency conflict at {key}: {previous[0]} and {role}")
            continue
        target = destination / relative
        if target.exists() or target.is_symlink():
            raise RuntimeError(f"Toolchain dependency path collision at {key} from {role}")
        if state[0] == "directory":
            target.mkdir()
            shutil.copystat(path, target)
        elif state[0] == "link":
            target.symlink_to(path.readlink(), target_is_directory=path.is_dir())
        else:
            shutil.copy2(path, target)
        entries[key] = (role, state)


def _publish(distribution: pathlib.Path, output: pathlib.Path, build: pathlib.Path) -> None:
    backup = build / "previous-runtime"
    if backup.exists() or backup.is_symlink():
        raise RuntimeError(f"A preserved toolchain backup requires recovery: {backup}")
    if output.exists():
        _rename_runtime(output, backup, "runtime backup")
    try:
        _rename_runtime(distribution, output, "runtime publication")
        smoke_bundle(output, build)
    except BaseException as error:
        try:
            _remove_owned(output)
            if backup.exists():
                _rename_runtime(backup, output, "runtime rollback")
        except (OSError, RuntimeError) as rollback_error:
            backup_detail = f"\nPrevious runtime backup: {backup}" if backup.exists() else ""
            raise RuntimeError(
                f"Toolchain publication failed: {error}\nRollback also failed: {rollback_error}{backup_detail}"
            ) from error
        raise
    _remove_owned(backup)


def build(project: pathlib.Path) -> None:
    if sys.version_info[:2] != (3, 12):
        raise RuntimeError("Toolchain requires a Python 3.12 virtual environment. Run tools/setup_python again.")
    source = project / "Toolchain"
    entries = project / "tools" / "toolchain_entrypoints"
    if not (source / "cli" / "catalogue.py").is_file():
        raise RuntimeError(f"Toolchain source was not found: {source}")
    output = project / ".tools" / "Toolchain"
    intermediate = project / ".tools" / "build" / "Toolchain"
    versions = _versions()
    source_hash = _source_hash(project, versions)
    try:
        validate_bundle(output, source_hash)
    except (OSError, ValueError, RuntimeError):
        pass
    else:
        print(f"Using current Toolchain: {output}", flush=True)
        return
    if output.is_symlink():
        raise RuntimeError(f"Toolchain output directory must not be a link: {output}")
    backup = intermediate / "previous-runtime"
    if backup.exists() or backup.is_symlink():
        raise RuntimeError(f"A preserved toolchain backup requires recovery: {backup}")
    intermediate.mkdir(parents=True, exist_ok=True)
    distribution = intermediate / "Toolchain.dist"
    _remove_owned(distribution)
    distribution.mkdir()
    merged_entries: dict[str, tuple[str, tuple[object, ...]]] = {}
    print("Building Toolchain runtime bundle...", flush=True)
    for role in TOOL_ROLES:
        role_build = intermediate / role
        role_build.mkdir(exist_ok=True)
        role_distribution = role_build / f"ludork-{role}.dist"
        _remove_owned(role_distribution)
        command = [
            sys.executable, "-m", "nuitka", f"--mode={BUILD_MODE}", "--assume-yes-for-downloads",
            f"--output-folder-name=ludork-{role}", f"--output-dir={role_build}",
            f"--output-filename={executable_name(role)}", f"--report={role_build / 'compilation-report.xml'}",
            *(f"--include-module=Toolchain.{module}" for module in sorted(set(COMMANDS[role].values()))),
            str(entries / f"ludork-{role}.py"),
        ]
        if sys.platform == "win32":
            command.append("--reproducible=yes")
        print(f"Building ludork-{role}...", flush=True)
        subprocess.run(command, cwd=project, check=True)
        _merge_distribution(role_distribution, distribution, role, merged_entries)
    smoke_bundle(distribution, intermediate)
    if source_hash != _source_hash(project, _versions()):
        raise RuntimeError("Toolchain inputs changed during compilation; rebuild the tools.")
    (distribution / SOURCE_STAMP_NAME).write_text(source_hash + "\n", encoding="utf-8", newline="\n")
    (distribution / VERSION_REPORT_NAME).write_text(
        "".join(f"{name}: {value}\n" for name, value in versions.items()), encoding="utf-8", newline="\n",
    )
    write_manifest(distribution)
    validate_bundle(distribution, source_hash)
    output.parent.mkdir(parents=True, exist_ok=True)
    _publish(distribution, output, intermediate)
    print(f"Toolchain ready: {output}", flush=True)


def main() -> int:
    try:
        build(PROJECT_ROOT)
        return 0
    except KeyboardInterrupt:
        print("Toolchain build cancelled.", file=sys.stderr)
        return 130
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
