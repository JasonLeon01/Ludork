from __future__ import annotations

import pathlib
import re
from dataclasses import dataclass

from .lua_syntax.constants import LUA_NAME_PATTERN
from .lua_syntax.identifiers import is_identifier
from .lua_syntax.lexer import tokenize
from .lua_syntax.literals import NEWLINE
from .lua_syntax.lua_token import LuaToken
from .lua_syntax.syntax_error import LuaSyntaxError
from .ui_assets import _is_link
from .ui_property_values import UiAssetError


OUTPUT_ROOT = pathlib.Path("Scripts/stub/Internal/UIWindows")
_NAME = LUA_NAME_PATTERN.pattern
_METHOD = re.compile(
    rf"(?P<docs>(?:^[ \t]*---[^\n]*\n)*)"
    rf"^[ \t]*(?P<definition>function)[ \t]+(?P<owner>{_NAME})(?P<separator>[:.])"
    rf"(?P<name>{_NAME})\s*\((?P<arguments>[^)]*)\)",
    re.MULTILINE,
)


@dataclass(frozen=True)
class Method:
    name: str
    separator: str
    arguments: str
    docs: str


def _error(path: pathlib.Path, token: LuaToken, message: str) -> UiAssetError:
    return UiAssetError(f"{path}:{token.line}:{token.column}: {message}")


def _read_source(path: pathlib.Path) -> str:
    with path.open(encoding="utf-8", newline="") as source:
        return NEWLINE.sub("\n", source.read())


def _references(path: pathlib.Path, tokens: list[LuaToken]) -> dict[str, str]:
    references: dict[str, str] = {}
    depth = 0
    for index in range(len(tokens) - 3):
        token = tokens[index]
        if token.kind != "string":
            if token.lexeme in {"end", "until", "elseif"}:
                depth -= 1
            elif token.lexeme in {"function", "then", "do", "repeat"}:
                depth += 1
        if depth != 0:
            continue
        if tokens[index].lexeme != "local" or tokens[index + 2].lexeme != "=":
            continue
        name = tokens[index + 1].lexeme
        position = index + 3
        if tokens[position].lexeme == "require":
            position += 1
            if position >= len(tokens):
                continue
            if tokens[position].lexeme == "(":
                position += 1
            if position < len(tokens) and tokens[position].kind == "string":
                literal = tokens[position]
                assert literal.string_value is not None
                try:
                    references[name] = literal.string_value.decode("utf-8")
                except UnicodeDecodeError as error:
                    raise _error(path, literal, "UI module names must be UTF-8 text") from error
        elif tokens[position].lexeme in references:
            value = references[tokens[position].lexeme]
            position += 1
            while position + 1 < len(tokens) and tokens[position].lexeme == ".":
                value += "." + tokens[position + 1].lexeme
                position += 2
            references[name] = value
    return references


def _arguments(path: pathlib.Path, tokens: list[LuaToken], start: int) -> list[list[LuaToken]]:
    arguments: list[list[LuaToken]] = [[]]
    depth = 0
    for index in range(start, len(tokens)):
        token = tokens[index]
        if token.lexeme == ")" and token.kind != "string" and depth == 0:
            if any(item.lexeme != ";" for item in tokens[index + 1:]):
                raise _error(path, token, "Ui.DefineWindow must be the module's final return")
            return arguments
        if token.lexeme == "," and token.kind != "string" and depth == 0:
            arguments.append([])
        else:
            arguments[-1].append(token)
            if token.lexeme in {"(", "{", "["} and token.kind != "string":
                depth += 1
            elif token.lexeme in {")", "}", "]"} and token.kind != "string":
                depth -= 1
    raise _error(path, tokens[start - 1], "Unclosed Ui.DefineWindow call")


def _reference(argument: list[LuaToken], references: dict[str, str]) -> str | None:
    if not argument or argument[0].lexeme not in references:
        return None
    value = references[argument[0].lexeme]
    for index in range(1, len(argument), 2):
        if index + 1 >= len(argument) or argument[index].lexeme != ".":
            return None
        value += "." + argument[index + 1].lexeme
    return value


def _controller_type(text: str, name: str, module: str) -> str:
    declaration = re.search(
        rf"(?P<docs>(?:^---[^\n]*\n)*)^local\s+{re.escape(name)}\s*=",
        text, re.MULTILINE,
    )
    if declaration:
        annotation = re.search(r"^---@class\s+([\w.]+)", declaration["docs"], re.MULTILINE)
        if annotation:
            return annotation[1]
    return module + ".Controller"


def _methods(text: str, owner: str, tokens: list[LuaToken] | None = None) -> dict[str, Method]:
    function_positions = {token.start for token in tokens if token.lexeme == "function" and token.kind != "string"} if tokens is not None else None
    return {
        match["name"]: Method(
            match["name"], match["separator"],
            ", ".join(argument.strip() for argument in match["arguments"].split(",") if argument.strip()),
            match["docs"].rstrip(),
        )
        for match in _METHOD.finditer(text)
        if match["owner"] == owner
        and (function_positions is None or match.start("definition") in function_positions)
    }


def _constant_types(path: pathlib.Path, tokens: list[LuaToken], owner: str, header: str) -> dict[str, str]:
    constants = dict(re.findall(r"^---@field\s+([A-Z][A-Z0-9_]*)\s+([^\n]+)", header, re.MULTILINE))
    for index in range(len(tokens) - 4):
        if tokens[index].lexeme != owner or tokens[index + 1].lexeme != "." or tokens[index + 3].lexeme != "=":
            continue
        name = tokens[index + 2].lexeme
        if not re.fullmatch(r"[A-Z][A-Z0-9_]*", name) or name in constants:
            continue
        value = tokens[index + 4]
        if value.kind == "string":
            constants[name] = "string"
        elif value.lexeme in {"true", "false"}:
            constants[name] = "boolean"
        elif re.fullmatch(r"\d+", value.lexeme):
            constants[name] = "integer"
        elif re.fullmatch(r"\d+\.\d+", value.lexeme):
            constants[name] = "number"
        elif value.lexeme == "{":
            constants[name] = "table"
        else:
            raise _error(path, value, f"Declare the type of UI constant {owner}.{name} in the Controller stub")
    return constants


def _declaration(method: Method, owner: str, *, name: str | None = None,
                 separator: str | None = None, arguments: str | None = None,
                 extra: list[str] | None = None) -> list[str]:
    return [
        *([method.docs] if method.docs else []),
        *(extra or []),
        f"function {owner}{separator or method.separator}{name or method.name}"
        f"({method.arguments if arguments is None else arguments}) end",
        "",
    ]


def _format_annotations(lines: list[str]) -> str:
    text = "\n".join(lines)
    for tag in ("param", "field"):
        pattern = re.compile(rf"(?:^---@{tag}[ \t]+[^\n]+\n)+", re.MULTILINE)

        def align(match: re.Match[str]) -> str:
            entries = [line.split(None, 2) for line in match[0].splitlines()]
            width = max(len(entry[1]) for entry in entries)
            return "".join(f"{entry[0]} {entry[1].ljust(width)} {entry[2]}\n" for entry in entries)

        text = pattern.sub(align, text)
    return text


def _render(path: pathlib.Path, text: str, relative: pathlib.Path,
            tokens: list[LuaToken], arguments: list[list[LuaToken]], references: dict[str, str],
            view_types: dict[str, str], marker: str, stub: pathlib.Path) -> str:
    if len(arguments) not in {2, 3} or any(not argument for argument in arguments):
        raise _error(path, tokens[-1], "Ui.DefineWindow expects a View, Controller and optional native base")
    view_module = _reference(arguments[0], references)
    if view_module not in view_types:
        raise _error(path, arguments[0][0], "Ui.DefineWindow View must reference a generated UI asset module")
    view_type = view_types[view_module]
    base_type = _reference(arguments[2], references) if len(arguments) == 3 and [token.lexeme for token in arguments[2]] != ["nil"] else "Engine.Canvas"
    if base_type is None:
        raise _error(path, arguments[2][0], "Ui.DefineWindow native base must be a directly imported class")
    if len(arguments[1]) == 1 and is_identifier(arguments[1][0].lexeme):
        controller_name = arguments[1][0].lexeme
    elif [token.lexeme for token in arguments[1]] == ["{", "}"]:
        controller_name = ""
    else:
        raise _error(path, arguments[1][0], "Ui.DefineWindow Controller must be a local definition table or an empty table")
    module = relative.with_suffix("").as_posix().replace("/", ".")
    controller_type = _controller_type(text, controller_name, module)
    window_type = controller_type.removesuffix(".Controller") if controller_type.endswith(".Controller") else module
    source_methods = _methods(text, controller_name, tokens)
    methods = source_methods
    header = ""
    if stub.is_file():
        contract = _read_source(stub)
        declaration = re.search(
            rf"(?P<docs>(?:^---[^\n]*\n)*)^---@class\s+{re.escape(controller_type)}(?=[:\s]|$)"
            rf"[^\n]*\n(?P<fields>(?:^---[^\n]*\n)*)^local\s+(?P<owner>{_NAME})\s*=",
            contract, re.MULTILINE,
        )
        if declaration is None:
            raise UiAssetError(f"{stub}: UI Controller declaration {controller_type} was not found")
        if re.search(r"^---@meta[ \t]+\S|^return\s", contract, re.MULTILINE):
            raise UiAssetError(f"{stub}: Window stubs declare private Controller types with bare @meta and no return; the UI module declaration is generated")
        header = declaration["fields"]
        methods = _methods(contract, declaration["owner"])
        declared_host = re.search(r"^---@field\s+host\s+([\w.]+)", header, re.MULTILINE)
        if declared_host:
            window_type = declared_host[1]
        description = declaration["docs"].rstrip()
    else:
        description = ""
    constructor = methods.get("init", Method("init", ":", "", ""))
    if "init" in source_methods and constructor.arguments != source_methods["init"].arguments:
        raise UiAssetError(f"{stub}: Controller:init arguments do not match {path}")
    constants = _constant_types(path, tokens, controller_name, header)
    lines = [marker, "---@meta " + module, ""]
    if description:
        lines.append(description)
    lines.extend([
        f"---@class {window_type}: {base_type}, Internal.UIBase.Ui.Window",
        f"---@field ui {view_type}",
        *[f"---@field {name} {type_name}" for name, type_name in sorted(constants.items())],
        "local Window = {}", "",
    ])
    lines.extend(_declaration(constructor, "Window", name="new", separator=".", extra=[f"---@return {window_type}"]))
    view_arguments = "ui" + (", " + constructor.arguments if constructor.arguments else "")
    lines.extend(_declaration(constructor, "Window", name="FromView", separator=".",
                              arguments=view_arguments, extra=[f"---@param ui {view_type}", f"---@return {window_type}"]))
    for name, method in sorted(methods.items()):
        if name.startswith("_") or name in {"init", "ready", "FromView", "Publish", "mount", "unmount"}:
            continue
        if name not in source_methods:
            continue
        lines.extend(_declaration(method, "Window"))
    lines.extend(["return Window", ""])
    return _format_annotations(lines)


def window_outputs(
    project_root: pathlib.Path,
    view_types: dict[str, str],
    marker: str,
    inputs: dict[pathlib.Path, bytes | None],
) -> dict[pathlib.Path, bytes]:
    scripts = project_root / "Scripts"
    source = scripts / "Source"
    outputs: dict[pathlib.Path, bytes] = {}
    if not source.is_dir():
        return outputs
    window_types: dict[str, pathlib.Path] = {}
    for path in sorted(source.rglob("*.lua"), key=lambda path: path.as_posix()):
        relative = path.relative_to(scripts)
        if path.name.endswith((".d.lua", "_meta.lua")):
            continue
        for item in (path, *path.relative_to(project_root).parents):
            target = item if item.is_absolute() else project_root / item
            if _is_link(target):
                raise UiAssetError(f"UI generation does not follow links: {target}")
        text = _read_source(path)
        if "DefineWindow" not in text:
            continue
        try:
            tokens = tokenize(text)
        except LuaSyntaxError as error:
            raise UiAssetError(f"{path}:{error}") from error
        references = _references(path, tokens)
        calls = [
            index for index in range(len(tokens) - 3)
            if references.get(tokens[index].lexeme) == "Internal.UIBase.Ui"
            and [token.lexeme for token in tokens[index + 1:index + 4]] == [".", "DefineWindow", "("]
        ]
        if not calls:
            continue
        if len(calls) != 1 or calls[0] == 0 or tokens[calls[0] - 1].lexeme != "return":
            raise _error(path, tokens[calls[0]], "Declare a UI module with one final return Ui.DefineWindow(...) call")
        arguments = _arguments(path, tokens, calls[0] + 4)
        stub = scripts / "stub" / relative.with_suffix(".d.lua")
        for item in (stub, *stub.relative_to(project_root).parents):
            target = item if item.is_absolute() else project_root / item
            if _is_link(target):
                raise UiAssetError(f"UI generation does not follow links: {target}")
        inputs[path.relative_to(project_root)] = path.read_bytes()
        inputs[stub.relative_to(project_root)] = stub.read_bytes() if stub.is_file() else None
        content = _render(path, text, relative, tokens, arguments, references, view_types, marker, stub)
        window_type = re.search(r"^---@class ([\w.]+):", content, re.MULTILINE)[1]
        if window_type in window_types or window_type in view_types.values():
            other = window_types.get(window_type, "a generated View")
            raise UiAssetError(f"UI window type {window_type} from {path} conflicts with {other}")
        window_types[window_type] = path
        output = OUTPUT_ROOT / path.relative_to(source).with_suffix(".d.lua")
        outputs[output] = content.encode("utf-8")
    return outputs
