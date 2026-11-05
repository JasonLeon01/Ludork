from __future__ import annotations

import pathlib
import posixpath
import subprocess
from collections.abc import Iterator

from .tool_paths import TOOL_ROLES, executable_name


MACHO_MAGICS = {b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe", b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",
                b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca", b"\xca\xfe\xba\xbf", b"\xbf\xba\xfe\xca"}
SYSTEM_LIBRARY_ROOTS = ("/usr/lib/", "/System/Library/", "/Library/Apple/System/Library/")


def _is_system_path(value: str) -> bool:
    return posixpath.normpath(value).startswith(SYSTEM_LIBRARY_ROOTS)


def _load_paths(path: pathlib.Path) -> tuple[list[str], list[str]]:
    result = subprocess.run(["otool", "-l", str(path)], capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise ValueError(f"Cannot inspect runtime bundle dependencies: {path}: {result.stderr}")
    dependencies: list[str] = []
    rpaths: list[str] = []
    command = ""
    for line in result.stdout.splitlines():
        line = line.strip()
        if line.startswith("cmd "):
            command = line[4:]
        elif command == "LC_RPATH" and line.startswith("path "):
            rpaths.append(line[5:].rsplit(" (offset ", 1)[0])
        elif command in {"LC_LOAD_DYLIB", "LC_LOAD_WEAK_DYLIB", "LC_REEXPORT_DYLIB", "LC_LOAD_UPWARD_DYLIB"} and line.startswith("name "):
            dependencies.append(line[5:].rsplit(" (offset ", 1)[0])
    return dependencies, rpaths


def _relative_path(value: str, loader: pathlib.Path, root: pathlib.Path) -> pathlib.Path | None:
    for prefix, directory in (("@loader_path", loader), ("@executable_path", root)):
        if value == prefix or value.startswith(prefix + "/"):
            return directory / value[len(prefix):].lstrip("/")
    return None


def _search_paths(values: list[str], loader: pathlib.Path, root: pathlib.Path,
                  inherited: tuple[pathlib.Path | str, ...]) -> tuple[pathlib.Path | str, ...]:
    paths: list[pathlib.Path | str] = []
    for value in values:
        resolved = _relative_path(value, loader, root)
        if resolved is not None:
            paths.append(resolved.resolve())
        elif value.startswith("/") or pathlib.Path(value).is_absolute():
            paths.append(pathlib.Path(value))
        else:
            paths.append(value)
    return tuple(dict.fromkeys((*paths, *inherited)))


def _dependency_candidates(dependency: str, path: pathlib.Path, root: pathlib.Path,
                           search_paths: tuple[pathlib.Path | str, ...]) -> Iterator[pathlib.Path]:
    if dependency.startswith("@rpath/"):
        for directory in search_paths:
            if isinstance(directory, str):
                raise ValueError(f"Unsupported runtime bundle rpath for {path}: {directory}: {dependency}")
            yield directory / dependency[len("@rpath/"):]
    else:
        resolved = _relative_path(dependency, path.parent, root)
        if resolved is not None:
            yield resolved


def _validate_graph(entry: pathlib.Path, root: pathlib.Path,
                    binaries: dict[pathlib.Path, tuple[list[str], list[str]]],
                    inherited: tuple[pathlib.Path | str, ...] = ()) -> set[pathlib.Path]:
    reached: set[pathlib.Path] = set()
    active: set[pathlib.Path] = set()
    visited: set[tuple[pathlib.Path, tuple[pathlib.Path | str, ...]]] = set()

    def visit(path: pathlib.Path, parents: tuple[pathlib.Path | str, ...]) -> None:
        dependencies, rpaths = binaries[path]
        search_paths = _search_paths(rpaths, path.parent, root, parents)
        state = (path, search_paths)
        if path in active or state in visited:
            return
        active.add(path)
        for dependency in dependencies:
            if _is_system_path(dependency):
                continue
            candidates = _dependency_candidates(dependency, path, root, search_paths)
            candidate = next((candidate for candidate in candidates
                              if candidate.is_file() or _is_system_path(candidate.as_posix())), None)
            if candidate is not None and _is_system_path(candidate.as_posix()):
                continue
            target = None if candidate is None else candidate.resolve()
            if target is None or not target.is_relative_to(root) or target not in binaries:
                raise ValueError(f"Runtime bundle dependency is missing or not relocatable: {path}: {dependency}")
            visit(target, search_paths)
        active.remove(path)
        visited.add(state)
        reached.add(path)

    visit(entry, inherited)
    return reached


def validate_dependencies(root: pathlib.Path) -> None:
    root = root.resolve()
    binaries: dict[pathlib.Path, tuple[list[str], list[str]]] = {}
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as stream:
            is_macho = stream.read(4) in MACHO_MAGICS
        if is_macho:
            binaries[path] = _load_paths(path)
    entries: list[pathlib.Path] = []
    reached: set[pathlib.Path] = set()
    for role in TOOL_ROLES:
        path = root / executable_name(role)
        if path not in binaries:
            raise ValueError(f"Runtime bundle entry is not Mach-O: {path}")
        entries.append(path)
        reached.update(_validate_graph(path, root, binaries))
    for path in binaries:
        if path in reached:
            continue
        failures: list[str] = []
        for entry in entries:
            rpaths = _search_paths(binaries[entry][1], entry.parent, root, ())
            try:
                loaded = _validate_graph(path, root, binaries, rpaths)
            except ValueError as error:
                failures.append(f"{entry.name}: {error}")
            else:
                reached.update(loaded)
                break
        else:
            raise ValueError(f"Runtime bundle module cannot load from any entry: {path}\n" + "\n".join(failures))
