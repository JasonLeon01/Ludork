from __future__ import annotations

import hashlib
import importlib.metadata
import json
import pathlib
import platform
import shutil
import ssl
import subprocess
import sys


PROJECT_ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from ScriptTools.runtime_bundle import (
    BUILD_MODE,
    SOURCE_STAMP_NAME,
    VERSION_REPORT_NAME,
    executable_name,
    validate_bundle,
    write_manifest,
)


def _versions() -> dict[str, str]:
    return {
        "Build mode": BUILD_MODE,
        "Platform": f"{platform.system()}-{platform.machine()}",
        "CPython": platform.python_version(),
        "OpenSSL": ssl.OPENSSL_VERSION,
        "Nuitka": importlib.metadata.version("Nuitka"),
        "Pillow": importlib.metadata.version("Pillow"),
    }


def _source_hash(project: pathlib.Path, versions: dict[str, str]) -> str:
    paths = sorted((project / "ScriptTools").rglob("*.py"))
    paths.extend(project / relative for relative in (
        "requirements.txt", "tools/build_script_tools.py",
        "tools/build_script_tools.sh", "tools/build_script_tools.bat",
    ))
    digest = hashlib.sha256(json.dumps(versions, sort_keys=True).encode("utf-8"))
    for path in paths:
        digest.update(path.relative_to(project).as_posix().encode("utf-8") + b"\0")
        digest.update(path.read_bytes() + b"\0")
    return digest.hexdigest()


def _remove_owned(path: pathlib.Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)


def _smoke(directory: pathlib.Path, working_directory: pathlib.Path) -> None:
    result = subprocess.run(
        [str(directory / executable_name()), "packaging-constants", "list", "editor-cache-directory"],
        cwd=working_directory,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0 or not result.stdout.strip():
        raise RuntimeError(f"ScriptTools runtime smoke check failed: {result.stdout}{result.stderr}")


def _publish(distribution: pathlib.Path, output: pathlib.Path, build: pathlib.Path) -> None:
    backup = build / "previous-runtime"
    _remove_owned(backup)
    if output.exists():
        output.rename(backup)
    try:
        distribution.rename(output)
        _smoke(output, build)
    except BaseException:
        _remove_owned(output)
        if backup.exists():
            backup.rename(output)
        raise
    _remove_owned(backup)


def build(project: pathlib.Path) -> None:
    if sys.version_info[:2] != (3, 12):
        raise RuntimeError("ScriptTools requires a Python 3.12 virtual environment. Run tools/setup_python again.")
    source = project / "ScriptTools"
    if not (source / "__main__.py").is_file():
        raise RuntimeError(f"ScriptTools source was not found: {source}")
    output = project / ".tools" / "ScriptTools"
    intermediate = project / ".tools" / "build" / "ScriptTools"
    versions = _versions()
    source_hash = _source_hash(project, versions)
    try:
        validate_bundle(output, source_hash)
    except (OSError, ValueError, RuntimeError):
        pass
    else:
        print(f"Using current ScriptTools: {output / executable_name()}", flush=True)
        return
    if output.is_symlink():
        raise RuntimeError(f"ScriptTools output directory must not be a link: {output}")
    (output / SOURCE_STAMP_NAME).unlink(missing_ok=True)
    intermediate.mkdir(parents=True, exist_ok=True)
    distribution = intermediate / "ScriptTools.dist"
    _remove_owned(distribution)
    command = [
        sys.executable, "-m", "nuitka", f"--mode={BUILD_MODE}",
        "--assume-yes-for-downloads", "--include-package=ScriptTools",
        "--output-folder-name=ScriptTools", f"--output-dir={intermediate}",
        f"--output-filename={executable_name()}", str(source / "__main__.py"),
    ]
    print("Building ScriptTools runtime bundle...", flush=True)
    subprocess.run(command, cwd=project, check=True)
    _smoke(distribution, intermediate)
    if source_hash != _source_hash(project, _versions()):
        raise RuntimeError("ScriptTools inputs changed during compilation; rebuild the tools.")
    (distribution / SOURCE_STAMP_NAME).write_text(source_hash + "\n", encoding="utf-8")
    (distribution / VERSION_REPORT_NAME).write_text(
        "".join(f"{name}: {value}\n" for name, value in versions.items()), encoding="utf-8",
    )
    write_manifest(distribution)
    validate_bundle(distribution, source_hash)
    output.parent.mkdir(parents=True, exist_ok=True)
    _publish(distribution, output, intermediate)
    print(f"ScriptTools ready: {output / executable_name()}", flush=True)


def main() -> int:
    try:
        build(PROJECT_ROOT)
        return 0
    except KeyboardInterrupt:
        print("ScriptTools build cancelled.", file=sys.stderr)
        return 130
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
