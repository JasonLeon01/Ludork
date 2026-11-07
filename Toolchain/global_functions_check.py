from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

from Toolchain import tool_process
from Toolchain.core_bindgen.annotations import parse_header
from Toolchain.core_bindgen.context import GeneratorContext
from Toolchain.core_bindgen.scopes import register_headers
from Toolchain.lua_syntax.identifiers import is_identifier
from Toolchain.lua_syntax.lexer import tokenize
from Toolchain.lua_syntax.lua_token import LuaToken
from Toolchain.lua_syntax.syntax_error import LuaSyntaxError


NATIVE_HEADERS = Path("Game/Engine/Source/GlobalFunctions/include")
LUA_MODULES = Path("Game/Scripts/GlobalFunctions")


def _native_exports(root: Path) -> dict[tuple[str, ...], Path]:
    headers = sorted((root / NATIVE_HEADERS).rglob("*.hpp"))
    if not headers:
        raise ValueError(f"No native GlobalFunctions headers found in {NATIVE_HEADERS}")
    context = GeneratorContext()
    register_headers(context, headers)
    exports: dict[tuple[str, ...], Path] = {}
    for header in headers:
        _, _, functions = parse_header(context, header)
        for function in functions:
            group = function.options.get("group")
            name = function.options.get("name", function.name)
            if group is not None:
                exports[(group,)] = header
                exports[(group, name)] = header
            else:
                exports[(name,)] = header
    return exports


def _module_scope_requires(tokens: list[LuaToken]) -> list[int]:
    blocks: list[str] = []
    pending_do = 0
    positions: list[int] = []
    for index, token in enumerate(tokens):
        if token.kind == "string":
            continue
        value = token.lexeme
        if value == "require" and "function" not in blocks:
            previous = tokens[index - 1].lexeme if index else ""
            if previous not in {".", ":"}:
                positions.append(token.line)
        if value in {"function", "if", "for", "while", "repeat"}:
            blocks.append(value)
            if value in {"for", "while"}:
                pending_do += 1
        elif value == "do":
            if pending_do:
                pending_do -= 1
            else:
                blocks.append(value)
        elif value == "end" and blocks:
            blocks.pop()
        elif value == "until" and blocks and blocks[-1] == "repeat":
            blocks.pop()
    return positions


def _lua_exports(tokens: list[LuaToken], module: tuple[str, ...]) -> set[tuple[str, ...]]:
    owner = module[-1]
    result = {module}
    for index in range(len(tokens) - 3):
        if (tokens[index].lexeme == "function"
                and tokens[index + 1].lexeme == owner
                and tokens[index + 2].lexeme == "."
                and tokens[index + 3].kind == "name"):
            result.add((*module, tokens[index + 3].lexeme))
        if (tokens[index].lexeme == owner
                and tokens[index + 1].lexeme == "."
                and tokens[index + 2].kind == "name"
                and tokens[index + 3].lexeme == "="):
            result.add((*module, tokens[index + 2].lexeme))
    return result


def check(root: Path) -> list[str]:
    native = _native_exports(root)
    messages: list[str] = []
    for path in sorted((root / LUA_MODULES).rglob("*.lua")):
        if path.name.endswith(("_meta.lua", ".d.lua")):
            continue
        relative = path.relative_to(root)
        module = path.relative_to(root / LUA_MODULES).with_suffix("").parts
        if any(not is_identifier(part) for part in module):
            messages.append(f"{relative}: module path contains an invalid Lua identifier")
            continue
        with path.open(encoding="utf-8", newline="") as script:
            content = script.read()
        try:
            tokens = tokenize(content)
        except LuaSyntaxError as error:
            messages.append(f"{relative}:{error}")
            continue
        for line in _module_scope_requires(tokens):
            messages.append(f"{relative}:{line}: require at module scope is forbidden")
        for export in sorted(_lua_exports(tokens, module)):
            native_source = native.get(export)
            if native_source is not None:
                name = "GlobalFunctions." + ".".join(export)
                messages.append(
                    f"{relative}: {name} conflicts with native binding "
                    f"{native_source.relative_to(root)}"
                )
    return messages


def _staged_snapshot(repository: Path, destination: Path) -> None:
    paths = tool_process.run(
        ["git", "ls-files", "--cached", "-z", "--", NATIVE_HEADERS.as_posix(), LUA_MODULES.as_posix()],
        cwd=repository, stdout=subprocess.PIPE, check=True,
    ).stdout.split(b"\0")
    for raw in paths:
        if not raw:
            continue
        relative = Path(raw.decode("utf-8"))
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Invalid staged path: {relative}")
        content = tool_process.run(["git", "show", ":" + relative.as_posix()],
                                   cwd=repository, stdout=subprocess.PIPE, check=True).stdout
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check Lua and native GlobalFunctions ownership")
    parser.add_argument("repository", type=Path)
    parser.add_argument("--staged", action="store_true", help="check the Git index before commit")
    args = parser.parse_args(arguments)
    repository = args.repository.expanduser().resolve()
    try:
        for relative in (NATIVE_HEADERS, LUA_MODULES):
            if not (repository / relative).is_dir():
                raise ValueError(f"GlobalFunctions input directory was not found: {repository / relative}")
        if args.staged:
            with tempfile.TemporaryDirectory(prefix="ludork-global-functions-") as directory:
                root = Path(directory)
                _staged_snapshot(repository, root)
                messages = check(root)
        else:
            messages = check(repository)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        return 1
    for message in messages:
        print(message, file=sys.stderr)
    if messages:
        return 1
    print("GlobalFunctions names and module loading are valid")
    return 0


if __name__ == "__main__":
    sys.exit(main())
