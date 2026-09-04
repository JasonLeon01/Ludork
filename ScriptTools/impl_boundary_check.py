from __future__ import annotations

import argparse
import pathlib
import re
from dataclasses import dataclass


class ImplBoundaryError(RuntimeError):
    pass


@dataclass(frozen=True)
class CppBoundary:
    host_source: pathlib.Path
    host_header: pathlib.Path
    host_types: tuple[str, ...]
    impl_directory: pathlib.Path


@dataclass(frozen=True)
class LuaBoundary:
    host_file: pathlib.Path
    host_module: str
    host_types: tuple[str, ...]
    impl_modules: tuple[str, ...]


CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".mm"}
HOST_MEMBER_CANDIDATE = re.compile(
    r"^[ \t]*(?:[^;(){}\n]+[ \t]+)?([A-Za-z_]\w*)\s*::\s*"
    r"(?:~?\w+|operator\s*[^\s(]+)\s*\(",
    re.MULTILINE,
)


def _line_number(text: str, position: int) -> int:
    return text.count("\n", 0, position) + 1


def _diagnostic(path: pathlib.Path, text: str, position: int, message: str) -> str:
    return f"{path}:{_line_number(text, position)}: {message}"


def _cpp_files(directory: pathlib.Path) -> list[pathlib.Path]:
    return sorted(path for path in directory.rglob("*") if path.suffix in CPP_SUFFIXES)


def _cpp_source_roots(project_root: pathlib.Path) -> list[pathlib.Path]:
    roots = [project_root / "Core", project_root / "Runtime"]
    return [root for root in roots if root.is_dir()]


def _cpp_include_roots(project_root: pathlib.Path) -> list[pathlib.Path]:
    roots = sorted((project_root / "Core").glob("*/include"))
    runtime_include = project_root / "Runtime" / "include"
    if runtime_include.is_dir():
        roots.append(runtime_include)
    return roots


def _without_cpp_comments_and_literals(text: str) -> str:
    pattern = re.compile(
        r'(?m)^\s*\#[^\n]*$|//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
        re.DOTALL,
    )

    def preserve_lines(match: re.Match[str]) -> str:
        return "\n" * match.group(0).count("\n")

    return pattern.sub(preserve_lines, text)


def _member_definitions(text: str) -> list[tuple[str, int]]:
    definitions: list[tuple[str, int]] = []
    for match in HOST_MEMBER_CANDIDATE.finditer(text):
        position = match.end()
        depth = 1
        while position < len(text) and depth > 0:
            if text[position] == "(":
                depth += 1
            elif text[position] == ")":
                depth -= 1
            position += 1
        if depth != 0:
            continue
        terminator = re.search(r"[;{}]", text[position:])
        if terminator is None or terminator.group(0) != "{":
            continue
        tail = text[position : position + terminator.start()]
        if (
            re.fullmatch(
                r"\s*(?:(?:const|noexcept|override|final)\s*|&{1,2}\s*)*",
                tail,
            )
            is None
        ):
            continue
        definitions.append((match.group(1), match.start()))
    return definitions


def _find_host_header(
    project_root: pathlib.Path,
    host_source: pathlib.Path,
) -> pathlib.Path:
    text = host_source.read_text(encoding="utf-8")
    includes = re.findall(r"#\s*include\s*[<\"]([^>\"]+)[>\"]", text)
    include_roots = _cpp_include_roots(project_root)
    candidates: list[pathlib.Path] = []
    for include in includes:
        include_path = pathlib.PurePosixPath(include)
        if include_path.stem != host_source.stem:
            continue
        for include_root in include_roots:
            candidate = include_root / pathlib.Path(*include_path.parts)
            if candidate.is_file():
                candidates.append(candidate)
    unique_candidates = sorted(set(candidates))
    if len(unique_candidates) != 1:
        display = ", ".join(str(path) for path in unique_candidates) or "none"
        raise ImplBoundaryError(
            f"Unable to resolve one public host header for {host_source}: {display}"
        )
    return unique_candidates[0]


def _discover_cpp_boundaries(project_root: pathlib.Path) -> list[CppBoundary]:
    source_roots = _cpp_source_roots(project_root)
    if not source_roots:
        raise ImplBoundaryError(f"C++ source roots were not found: {project_root}")
    boundaries: list[CppBoundary] = []
    for host_source in [
        path for source_root in source_roots for path in _cpp_files(source_root)
    ]:
        if host_source.suffix not in {".cc", ".cpp", ".cxx"}:
            continue
        impl_directory = host_source.with_suffix("")
        if not impl_directory.is_dir():
            continue
        host_header = _find_host_header(project_root, host_source)
        host_code = _without_cpp_comments_and_literals(
            host_source.read_text(encoding="utf-8")
        )
        defined_types = tuple(
            sorted({host_type for host_type, _ in _member_definitions(host_code)})
        )
        include_root = next(
            root
            for root in _cpp_include_roots(project_root)
            if root == host_header.parent or root in host_header.parents
        )
        bound_types: set[str] = set()
        for public_header in _cpp_files(include_root):
            header_text = public_header.read_text(encoding="utf-8")
            bound_types.update(
                re.findall(
                    r"BIND_CLASS\s*\(.*?\)\s*class(?:\s+[A-Za-z_]\w*)*\s+([A-Za-z_]\w*)",
                    header_text,
                    re.DOTALL,
                )
            )
        host_types = tuple(name for name in defined_types if name in bound_types)
        if not host_types:
            raise ImplBoundaryError(
                f"No bound host member definitions were found in {host_source} "
                f"for {host_header}"
            )
        boundaries.append(
            CppBoundary(
                host_source=host_source,
                host_header=host_header,
                host_types=host_types,
                impl_directory=impl_directory,
            )
        )
    if not boundaries:
        raise ImplBoundaryError(
            "No C++ host/same-name implementation directories were found under "
            f"{', '.join(str(root) for root in source_roots)}"
        )
    return boundaries


def _check_cpp(
    boundary: CppBoundary,
    project_root: pathlib.Path,
) -> list[str]:
    diagnostics: list[str] = []
    header_name = boundary.host_header.name
    header_pattern = re.compile(
        rf"#\s*include\s*[<\"](?:[^>\"]*/)?{re.escape(header_name)}[>\"]"
    )
    upward_include_pattern = re.compile(r"#\s*include\s*[<\"]\.\./")
    for path in _cpp_files(boundary.impl_directory):
        text = path.read_text(encoding="utf-8")
        for pattern, message in (
            (header_pattern, f"impl must not include host header {header_name}"),
            (upward_include_pattern, "impl must not include through ../"),
        ):
            for match in pattern.finditer(text):
                diagnostics.append(_diagnostic(path, text, match.start(), message))
        code = _without_cpp_comments_and_literals(text)
        for host_type in boundary.host_types:
            host_type_pattern = re.compile(rf"\b{re.escape(host_type)}\b")
            for match in host_type_pattern.finditer(code):
                diagnostics.append(
                    _diagnostic(
                        path,
                        code,
                        match.start(),
                        f"impl must not depend on host type {host_type}",
                    )
                )

    for path in [
        path
        for source_root in _cpp_source_roots(project_root)
        for path in _cpp_files(source_root)
    ]:
        if path == boundary.host_source or boundary.impl_directory in path.parents:
            continue
        text = path.read_text(encoding="utf-8")
        code = _without_cpp_comments_and_literals(text)
        for host_type, position in _member_definitions(code):
            if host_type not in boundary.host_types:
                continue
            diagnostics.append(
                _diagnostic(
                    path,
                    code,
                    position,
                    f"only host source {boundary.host_source} may define members of {host_type}",
                )
            )
    return diagnostics


def _lua_module(scripts_root: pathlib.Path, path: pathlib.Path) -> str:
    relative = path.relative_to(scripts_root).with_suffix("")
    return ".".join(relative.parts)


def _discover_lua_boundaries(project_root: pathlib.Path) -> list[LuaBoundary]:
    scripts_root = project_root / "Scripts"
    stub_root = scripts_root / "stub"
    if not scripts_root.is_dir() or not stub_root.is_dir():
        raise ImplBoundaryError(
            f"Lua runtime or stub root was not found under {scripts_root}"
        )
    boundaries: list[LuaBoundary] = []
    for host_file in sorted(scripts_root.rglob("*.lua")):
        if stub_root in host_file.parents or host_file.name.endswith("_meta.lua"):
            continue
        impl_directory = host_file.with_suffix("")
        if not impl_directory.is_dir():
            continue
        impl_files: list[pathlib.Path] = []
        for path in sorted(impl_directory.rglob("*.lua")):
            if path.name.endswith("_meta.lua"):
                continue
            relative = path.relative_to(scripts_root)
            stub_path = stub_root / relative.with_suffix(".d.lua")
            if not stub_path.is_file():
                impl_files.append(path)
        if not impl_files:
            continue
        host_text = host_file.read_text(encoding="utf-8")
        host_types = tuple(
            re.findall(r"(?m)^---@class\s+\(partial\)\s+([^\s:]+)", host_text)
        )
        if not host_types:
            raise ImplBoundaryError(
                f"Lua host with private implementation has no partial host type: {host_file}"
            )
        boundaries.append(
            LuaBoundary(
                host_file=host_file,
                host_module=_lua_module(scripts_root, host_file),
                host_types=host_types,
                impl_modules=tuple(
                    _lua_module(scripts_root, path) for path in impl_files
                ),
            )
        )
    if not boundaries:
        raise ImplBoundaryError(
            f"No Lua host/private implementation directories were found under {scripts_root}"
        )
    return boundaries


def _module_path(project_root: pathlib.Path, module: str) -> pathlib.Path:
    return (
        project_root / "Scripts" / pathlib.Path(*module.split(".")).with_suffix(".lua")
    )


def _require_aliases(text: str) -> dict[str, str]:
    pattern = re.compile(
        r"(?m)^local\s+([A-Za-z_]\w*)\s*=\s*require\s*\(\s*[\"']([^\"']+)[\"']\s*\)"
    )
    return {match.group(2): match.group(1) for match in pattern.finditer(text)}


def _check_lua(boundary: LuaBoundary, project_root: pathlib.Path) -> list[str]:
    diagnostics: list[str] = []
    host_text = boundary.host_file.read_text(encoding="utf-8")
    aliases = _require_aliases(host_text)
    impl_aliases: set[str] = set()

    for module in boundary.impl_modules:
        path = _module_path(project_root, module)
        if not path.is_file():
            diagnostics.append(f"Missing Lua impl module {module}: {path}")
            continue
        alias = aliases.get(module)
        if alias is not None:
            impl_aliases.add(alias)
        text = path.read_text(encoding="utf-8")
        host_require = re.compile(
            rf"\brequire\s*\(\s*[\"']{re.escape(boundary.host_module)}[\"']\s*\)"
        )
        for match in host_require.finditer(text):
            diagnostics.append(
                _diagnostic(
                    path,
                    text,
                    match.start(),
                    f"impl must not require host module {boundary.host_module}",
                )
            )
        for host_type in boundary.host_types:
            partial_host = re.compile(
                rf"---@class\s+\(partial\)\s+{re.escape(host_type)}\b"
            )
            for match in partial_host.finditer(text):
                diagnostics.append(
                    _diagnostic(
                        path,
                        text,
                        match.start(),
                        f"impl must not declare partial host type {host_type}",
                    )
                )

    class_calls = re.compile(r"\bclass\s*\((.*?)\)", re.DOTALL)
    for match in class_calls.finditer(host_text):
        arguments = match.group(1)
        for alias in sorted(impl_aliases):
            alias_match = re.search(rf"\b{re.escape(alias)}\b", arguments)
            if alias_match is not None:
                diagnostics.append(
                    _diagnostic(
                        boundary.host_file,
                        host_text,
                        match.start(1) + alias_match.start(),
                        f"host must delegate to impl module {alias}, not mix it into class()",
                    )
                )
    for alias in sorted(impl_aliases):
        copy_patterns = (
            re.compile(rf"\bpairs\s*\(\s*{re.escape(alias)}\s*\)"),
            re.compile(
                rf"(?m)^\s*[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?\s*=\s*{re.escape(alias)}\.[A-Za-z_]\w*\s*$"
            ),
        )
        for pattern in copy_patterns:
            for match in pattern.finditer(host_text):
                diagnostics.append(
                    _diagnostic(
                        boundary.host_file,
                        host_text,
                        match.start(),
                        f"host must delegate to impl module {alias}, not copy its methods",
                    )
                )
    return diagnostics


def verify_impl_boundaries(project_root: pathlib.Path) -> None:
    root = project_root.resolve()
    if not root.is_dir():
        raise ImplBoundaryError(f"Project root was not found: {root}")
    cpp = _discover_cpp_boundaries(root)
    lua = _discover_lua_boundaries(root)
    diagnostics: list[str] = []
    for boundary in cpp:
        diagnostics.extend(_check_cpp(boundary, root))
    for boundary in lua:
        diagnostics.extend(_check_lua(boundary, root))
    if diagnostics:
        raise ImplBoundaryError("\n".join(diagnostics))


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="ScriptTools impl-boundary-check")
    parser.add_argument("project_root", type=pathlib.Path)
    parsed = parser.parse_args(arguments)
    try:
        verify_impl_boundaries(parsed.project_root)
    except (OSError, ValueError, ImplBoundaryError) as exception:
        parser.exit(1, f"{exception}\n")
    print(f"Impl boundaries are valid: {parsed.project_root.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
