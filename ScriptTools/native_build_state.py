from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import platform
import sys
import tempfile

from .packaging_constants import EDITOR_CACHE_DIRECTORY


SOURCE_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".inl", ".ipp", ".m", ".mm", ".cmake", ".rc",
}
EXCLUDED_DIRECTORIES = {
    "thirdparty", "thirdpartysource", "build", "bin", ".cache",
    ".git", ".vs", ".idea", ".tools", ".venv", "obj", "dist",
}
EXCLUDED_ROOT_DIRECTORIES = {
    "assets", "data", "scripts", "intermediate", EDITOR_CACHE_DIRECTORY.casefold(), "cache",
    "log", "save", "binaries",
}


def _file_state(path: pathlib.Path) -> list[int]:
    info = path.stat()
    return [info.st_size, info.st_mtime_ns, info.st_ctime_ns, info.st_mode]


def _source_paths(project: pathlib.Path) -> list[pathlib.Path]:
    paths: list[pathlib.Path] = []

    def fail(error: OSError) -> None:
        raise error

    for directory, children, names in os.walk(project, onerror=fail):
        children[:] = [
            name for name in children
            if name.lower() not in EXCLUDED_DIRECTORIES
            and not (
                pathlib.Path(directory) == project
                and name.lower() in EXCLUDED_ROOT_DIRECTORIES
            )
            and not name.lower().startswith("cmake-build-")
        ]
        for name in names:
            path = pathlib.Path(directory) / name
            if name == "CMakeLists.txt" or path.suffix.lower() in SOURCE_EXTENSIONS:
                paths.append(path)
    if sys.platform == "win32":
        icon = project / "Assets" / "System" / "icon.ico"
        if icon.exists():
            paths.append(icon)
    return sorted(paths, key=lambda path: path.relative_to(project).as_posix())


def _inputs(project: pathlib.Path) -> dict:
    digest = hashlib.sha256()
    states = {}
    for path in _source_paths(project):
        relative = path.relative_to(project).as_posix()
        before = _file_state(path)
        with path.open("rb") as stream:
            content = hashlib.file_digest(stream, "sha256").hexdigest()
        if before != _file_state(path):
            raise ValueError(f"Native input changed while being read: {relative}")
        states[relative] = before
        digest.update(relative.encode("utf-8") + b"\0" + content.encode("ascii") + b"\n")
    project_file = project / "Main.proj"
    configuration_state = _file_state(project_file)
    with project_file.open(encoding="utf-8") as stream:
        configuration = json.load(stream)
    if configuration_state != _file_state(project_file):
        raise ValueError("Main.proj changed while being read.")
    if not isinstance(configuration, dict):
        raise ValueError("Main.proj must contain an object.")
    digest.update(b"ffmpeg:1" if configuration.get("ffmpeg") is True else b"ffmpeg:0")
    return {
        "digest": digest.hexdigest(),
        "files": states,
        "configurationState": configuration_state,
    }


def _artifacts(project: pathlib.Path, configuration: str) -> dict:
    directory = project / "bin" / configuration
    executable = directory / ("Main.exe" if sys.platform == "win32" else "Main")
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise ValueError(f"Native executable is missing or not executable: {executable}")
    files = {}
    for path in sorted(directory.iterdir()):
        if path == executable or path.suffix.lower() in {".dll", ".dylib", ".so"} or ".so." in path.name:
            files[path.name] = {
                "state": _file_state(path),
                "link": os.readlink(path) if path.is_symlink() else None,
            }
    return files


def _context(configuration: str) -> dict:
    return {
        "formatVersion": 1,
        "platform": sys.platform,
        "architecture": platform.machine().lower(),
        "configuration": configuration,
    }


def _paths(project: pathlib.Path, configuration: str) -> tuple[pathlib.Path, pathlib.Path]:
    directory = project / EDITOR_CACHE_DIRECTORY
    return (
        directory / f"NativeBuild-{configuration}.json",
        directory / f"NativeBuild-{configuration}.pending.json",
    )


def _write(path: pathlib.Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent, delete=False,
        ) as stream:
            temporary = pathlib.Path(stream.name)
            json.dump(value, stream, ensure_ascii=False, separators=(",", ":"))
            stream.write("\n")
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def begin(project: pathlib.Path, configuration: str) -> None:
    _, pending_path = _paths(project, configuration)
    _write(pending_path, _context(configuration))
    _write(pending_path, {**_context(configuration), "inputs": _inputs(project)})


def complete(project: pathlib.Path, configuration: str) -> bool:
    complete_path, pending_path = _paths(project, configuration)
    pending = json.loads(pending_path.read_text(encoding="utf-8"))
    current = _inputs(project)
    if pending != {**_context(configuration), "inputs": current}:
        print("Native inputs changed during the build; compile the project again.", file=sys.stderr)
        return False
    _write(complete_path, {
        **_context(configuration),
        "inputs": current["digest"],
        "artifacts": _artifacts(project, configuration),
    })
    pending_path.unlink(missing_ok=True)
    return True


def check(project: pathlib.Path, configuration: str) -> tuple[bool, str]:
    complete_path, pending_path = _paths(project, configuration)
    if pending_path.exists() or not complete_path.is_file():
        return False, "Compile the project successfully before playing."
    saved = json.loads(complete_path.read_text(encoding="utf-8"))
    if not isinstance(saved, dict) or any(
        saved.get(key) != value for key, value in _context(configuration).items()
    ):
        return False, "Compile this configuration for the current platform."
    if saved.get("inputs") != _inputs(project)["digest"]:
        return False, "Native sources changed; compile the project again."
    try:
        artifacts = _artifacts(project, configuration)
    except (OSError, ValueError):
        return False, "Native build outputs are missing; compile the project again."
    if saved.get("artifacts") != artifacts:
        return False, "Native build outputs changed; compile the project again."
    if pending_path.exists() or not complete_path.is_file():
        return False, "Compile the project successfully before playing."
    return True, ""


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools native-build-state")
    parser.add_argument("operation", choices=("begin", "complete", "check"))
    parser.add_argument("project", type=pathlib.Path)
    parser.add_argument("configuration", choices=("Debug", "Release"))
    args = parser.parse_args(arguments)
    project = args.project.expanduser().resolve()
    try:
        if not (project / "CMakeLists.txt").is_file():
            raise ValueError(f"CMakeLists.txt was not found: {project}")
        if args.operation == "begin":
            begin(project, args.configuration)
        elif args.operation == "complete":
            if not complete(project, args.configuration):
                return 1
        else:
            current, detail = check(project, args.configuration)
            print(json.dumps({"current": current, "detail": detail}))
        return 0
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
