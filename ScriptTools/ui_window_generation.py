from __future__ import annotations

import pathlib
import re
from dataclasses import dataclass

from .ui_assets import _is_link
from .ui_property_values import UiAssetError


OUTPUT_ROOT = pathlib.Path("Scripts/stub/Source/UIWindows")
_NAME = r"[A-Za-z_]\w*"
_METHOD = re.compile(
    rf"(?P<docs>(?:^[ \t]*---[^\n]*\n)*)"
    rf"^[ \t]*(?P<definition>function)[ \t]+(?P<owner>{_NAME})(?P<separator>[:.])"
    rf"(?P<name>{_NAME})\s*\((?P<arguments>[^)]*)\)",
    re.MULTILINE,
)


@dataclass(frozen=True)
class Token:
    value: str
    position: int
    string: bool = False


@dataclass(frozen=True)
class Method:
    name: str
    separator: str
    arguments: str
    docs: str


def _error(path: pathlib.Path, text: str, position: int, message: str) -> UiAssetError:
    return UiAssetError(f"{path}:{text.count(chr(10), 0, position) + 1}: {message}")


def _tokens(path: pathlib.Path, text: str) -> list[Token]:
    result: list[Token] = []
    position = 0
    while position < len(text):
        if text[position].isspace():
            position += 1
            continue
        start = position
        comment = text.startswith("--", position)
        if comment:
            position += 2
        long_open = re.match(r"\[(=*)\[", text[position:])
        if long_open:
            closing = "]" + long_open[1] + "]"
            content = position + len(long_open[0])
            end = text.find(closing, content)
            if end < 0:
                raise _error(path, text, start, "Unterminated Lua long string or comment")
            if not comment:
                result.append(Token(text[content:end], start, True))
            position = end + len(closing)
        elif comment:
            end = text.find("\n", position)
            position = len(text) if end < 0 else end + 1
        elif text[position] in {'"', "'"}:
            quote = text[position]
            position += 1
            content = position
            while position < len(text) and text[position] != quote:
                position += 2 if text[position] == "\\" else 1
            if position >= len(text):
                raise _error(path, text, start, "Unterminated Lua string")
            result.append(Token(text[content:position], start, True))
            position += 1
        else:
            name = re.match(r"[A-Za-z_]\w*|\d+(?:\.\d+)?|\.\.\.", text[position:])
            value = name[0] if name else text[position]
            result.append(Token(value, position))
            position += len(value)
    return result


def _references(tokens: list[Token]) -> dict[str, str]:
    references: dict[str, str] = {}
    depth = 0
    for index in range(len(tokens) - 3):
        token = tokens[index]
        if not token.string:
            if token.value in {"end", "until", "elseif"}:
                depth -= 1
            elif token.value in {"function", "then", "do", "repeat"}:
                depth += 1
        if depth != 0:
            continue
        if tokens[index].value != "local" or tokens[index + 2].value != "=":
            continue
        name = tokens[index + 1].value
        position = index + 3
        if tokens[position].value == "require":
            position += 1
            if position >= len(tokens):
                continue
            if tokens[position].value == "(":
                position += 1
            if position < len(tokens) and tokens[position].string:
                references[name] = tokens[position].value
        elif tokens[position].value in references:
            value = references[tokens[position].value]
            position += 1
            while position + 1 < len(tokens) and tokens[position].value == ".":
                value += "." + tokens[position + 1].value
                position += 2
            references[name] = value
    return references


def _arguments(path: pathlib.Path, text: str, tokens: list[Token], start: int) -> list[list[Token]]:
    arguments: list[list[Token]] = [[]]
    depth = 0
    for index in range(start, len(tokens)):
        token = tokens[index]
        if token.value == ")" and not token.string and depth == 0:
            if any(item.value != ";" for item in tokens[index + 1:]):
                raise _error(path, text, token.position, "Ui.DefineWindow must be the module's final return")
            return arguments
        if token.value == "," and not token.string and depth == 0:
            arguments.append([])
        else:
            arguments[-1].append(token)
            if token.value in {"(", "{", "["} and not token.string:
                depth += 1
            elif token.value in {")", "}", "]"} and not token.string:
                depth -= 1
    raise _error(path, text, tokens[start - 1].position, "Unclosed Ui.DefineWindow call")


def _reference(argument: list[Token], references: dict[str, str]) -> str | None:
    if not argument or argument[0].value not in references:
        return None
    value = references[argument[0].value]
    for index in range(1, len(argument), 2):
        if index + 1 >= len(argument) or argument[index].value != ".":
            return None
        value += "." + argument[index + 1].value
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


def _methods(text: str, owner: str, tokens: list[Token] | None = None) -> dict[str, Method]:
    function_positions = {token.position for token in tokens if token.value == "function" and not token.string} if tokens is not None else None
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


def _constant_types(path: pathlib.Path, text: str, tokens: list[Token], owner: str, header: str) -> dict[str, str]:
    constants = dict(re.findall(r"^---@field\s+([A-Z][A-Z0-9_]*)\s+([^\n]+)", header, re.MULTILINE))
    for index in range(len(tokens) - 4):
        if tokens[index].value != owner or tokens[index + 1].value != "." or tokens[index + 3].value != "=":
            continue
        name = tokens[index + 2].value
        if not re.fullmatch(r"[A-Z][A-Z0-9_]*", name) or name in constants:
            continue
        value = tokens[index + 4]
        if value.string:
            constants[name] = "string"
        elif value.value in {"true", "false"}:
            constants[name] = "boolean"
        elif re.fullmatch(r"\d+", value.value):
            constants[name] = "integer"
        elif re.fullmatch(r"\d+\.\d+", value.value):
            constants[name] = "number"
        elif value.value == "{":
            constants[name] = "table"
        else:
            raise _error(path, text, value.position, f"Declare the type of UI constant {owner}.{name} in the Controller stub")
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
            tokens: list[Token], arguments: list[list[Token]], references: dict[str, str],
            view_types: dict[str, str], marker: str, stub: pathlib.Path) -> str:
    if len(arguments) not in {2, 3} or any(not argument for argument in arguments):
        raise _error(path, text, tokens[-1].position, "Ui.DefineWindow expects a View, Controller and optional native base")
    view_module = _reference(arguments[0], references)
    if view_module not in view_types:
        raise _error(path, text, arguments[0][0].position, "Ui.DefineWindow View must reference a generated UI asset module")
    view_type = view_types[view_module]
    base_type = _reference(arguments[2], references) if len(arguments) == 3 and [token.value for token in arguments[2]] != ["nil"] else "Engine.Canvas"
    if base_type is None:
        raise _error(path, text, arguments[2][0].position, "Ui.DefineWindow native base must be a directly imported class")
    if len(arguments[1]) == 1 and re.fullmatch(_NAME, arguments[1][0].value):
        controller_name = arguments[1][0].value
    elif [token.value for token in arguments[1]] == ["{", "}"]:
        controller_name = ""
    else:
        raise _error(path, text, arguments[1][0].position, "Ui.DefineWindow Controller must be a local definition table or an empty table")
    module = relative.with_suffix("").as_posix().replace("/", ".")
    controller_type = _controller_type(text, controller_name, module)
    window_type = controller_type.removesuffix(".Controller") if controller_type.endswith(".Controller") else module
    source_methods = _methods(text, controller_name, tokens)
    methods = source_methods
    header = ""
    if stub.is_file():
        contract = stub.read_text(encoding="utf-8")
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
    constants = _constant_types(path, text, tokens, controller_name, header)
    lines = [marker, "---@meta " + module, ""]
    if description:
        lines.append(description)
    lines.extend([
        f"---@class {window_type}: {base_type}, Source.UIBase.Ui.Window",
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


def window_outputs(project_root: pathlib.Path, view_types: dict[str, str], marker: str) -> dict[pathlib.Path, bytes]:
    scripts = project_root / "Scripts"
    source = scripts / "Source"
    outputs: dict[pathlib.Path, bytes] = {}
    if not source.is_dir():
        return outputs
    window_types: dict[str, pathlib.Path] = {}
    for path in sorted(source.rglob("*.lua")):
        relative = path.relative_to(scripts)
        if path.name.endswith((".d.lua", "_meta.lua")):
            continue
        for item in (path, *path.relative_to(project_root).parents):
            target = item if item.is_absolute() else project_root / item
            if _is_link(target):
                raise UiAssetError(f"UI generation does not follow links: {target}")
        text = path.read_text(encoding="utf-8")
        if "DefineWindow" not in text:
            continue
        tokens = _tokens(path, text)
        references = _references(tokens)
        calls = [
            index for index in range(len(tokens) - 3)
            if references.get(tokens[index].value) == "Source.UIBase.Ui"
            and [token.value for token in tokens[index + 1:index + 4]] == [".", "DefineWindow", "("]
        ]
        if not calls:
            continue
        if len(calls) != 1 or calls[0] == 0 or tokens[calls[0] - 1].value != "return":
            raise _error(path, text, tokens[calls[0]].position, "Declare a UI module with one final return Ui.DefineWindow(...) call")
        arguments = _arguments(path, text, tokens, calls[0] + 4)
        stub = scripts / "stub" / relative.with_suffix(".d.lua")
        for item in (stub, *stub.relative_to(project_root).parents):
            target = item if item.is_absolute() else project_root / item
            if _is_link(target):
                raise UiAssetError(f"UI generation does not follow links: {target}")
        content = _render(path, text, relative, tokens, arguments, references, view_types, marker, stub)
        window_type = re.search(r"^---@class ([\w.]+):", content, re.MULTILINE)[1]
        if window_type in window_types or window_type in view_types.values():
            other = window_types.get(window_type, "a generated View")
            raise UiAssetError(f"UI window type {window_type} from {path} conflicts with {other}")
        window_types[window_type] = path
        output = OUTPUT_ROOT / path.relative_to(source).with_suffix(".d.lua")
        outputs[output] = content.encode("utf-8")
    return outputs
