from __future__ import annotations

import pathlib
import re
import shutil
import unicodedata

from .pack_error import PackError
from .packaging_constants import (
    EXIT_APP_NAME_UNCHANGED,
    EXIT_PROJECT,
)


_ARTIFACT_NAME_PATTERN = re.compile(r'[\x00-\x1f\x7f<>:"/\\|?*;]+')
_ARTIFACT_NAME_MAX_LENGTH = 80
_ARTIFACT_NAME_FALLBACK = "Ludork Game"
_TOKEN = re.compile(
    r"(?P<space>\s+)|(?P<comment>--\[(?P<comment_equals>=*)\[.*?\](?P=comment_equals)\]|--[^\n]*)"
    r"|(?P<string>\[(?P<equals>=*)\[.*?\](?P=equals)\]|\"(?:\\[\s\S]|[^\"\\])*\"|'(?:\\[\s\S]|[^'\\])*')"
    r"|(?P<name>[A-Za-z_]\w*)|(?P<symbol>\.\.|==|~=|<=|>=|\S)",
    re.DOTALL,
)
_ESCAPES = dict(zip("abfnrtv\\\"'", "\a\b\f\n\r\t\v\\\"'"))
_EXPRESSION_CONTINUATIONS = frozenset({
    "..", "+", "-", "*", "/", "%", "^", "&", "|", "~", "<", ">", "=",
    "==", "~=", "<=", ">=", "and", "or", ".", "[", "(", "{", ":", ",",
})


def _string_value(literal: str) -> str:
    if literal.startswith("["):
        opening = re.match(r"\[(=*)\[", literal)
        assert opening is not None
        value = literal[len(opening[0]):-len(opening[0])]
        return re.sub(r"^\r?\n", "", value).replace("\r\n", "\n")
    value = bytearray()
    content = literal[1:-1]
    position = 0
    while position < len(content):
        character = content[position]
        position += 1
        if character != "\\":
            if character in "\r\n":
                raise ValueError("unescaped newline in APP_NAME")
            value.extend(character.encode("utf-8"))
            continue
        escape = content[position]
        position += 1
        if escape in _ESCAPES:
            value.extend(_ESCAPES[escape].encode("utf-8"))
        elif escape in "\r\n":
            if escape == "\r" and content[position:position + 1] == "\n":
                position += 1
            value.append(10)
        elif escape == "z":
            while position < len(content) and content[position].isspace():
                position += 1
        elif escape == "x":
            digits = content[position:position + 2]
            if not re.fullmatch(r"[0-9a-fA-F]{2}", digits):
                raise ValueError("invalid hexadecimal escape in APP_NAME")
            value.append(int(digits, 16))
            position += 2
        elif escape == "u":
            match = re.match(r"\{([0-9a-fA-F]+)\}", content[position:])
            if match is None:
                raise ValueError("invalid Unicode escape in APP_NAME")
            codepoint = int(match[1], 16)
            if codepoint > 0x10FFFF:
                raise ValueError("invalid Unicode code point in APP_NAME")
            value.extend(chr(codepoint).encode("utf-8"))
            position += len(match[0])
        elif escape in "0123456789":
            match = re.match(r"[0-9]{1,3}", content[position - 1:])
            assert match is not None
            value.append(int(match[0]))
            position += len(match[0]) - 1
        else:
            raise ValueError(f"invalid escape \\{escape} in APP_NAME")
    return value.decode("utf-8")


def read_app_name(project: pathlib.Path) -> str:
    entry = project / "Scripts" / "Entry.lua"
    try:
        source = entry.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError) as error:
        raise PackError(f"Unable to read Lua entry script {entry}: {error}", EXIT_PROJECT) from error
    tokens = [match for match in _TOKEN.finditer(source) if match.lastgroup not in {"space", "comment"}]
    names: list[str] = []
    depth = 0
    table_scopes: list[int] = []
    for index, token in enumerate(tokens):
        if token.lastgroup == "string":
            continue
        word = token[0]
        previous = tokens[index - 1][0] if index else ""
        if word == "APP_NAME" and previous == "function":
            raise PackError(f"APP_NAME must not be reassigned: {entry}", EXIT_PROJECT)
        in_table_fields = bool(table_scopes) and table_scopes[-1] == depth
        assignment_end = index + 1
        if word == "APP_NAME" and not in_table_fields:
            while (assignment_end + 1 < len(tokens) and tokens[assignment_end][0] == ","
                    and tokens[assignment_end + 1].lastgroup == "name"):
                assignment_end += 2
        if (word == "APP_NAME" and assignment_end < len(tokens) and tokens[assignment_end][0] == "="
                and previous not in {"local", ".", ":"}
                and not (in_table_fields and previous in {"{", ",", ";"})):
            raise PackError(f"APP_NAME must not be reassigned: {entry}", EXIT_PROJECT)
        if word == "{":
            table_scopes.append(depth)
        elif word == "}":
            if table_scopes:
                table_scopes.pop()
        if word in {"end", "until", "elseif"}:
            depth -= 1
        if depth == 0 and word == "local" and index + 1 < len(tokens) and tokens[index + 1][0] == "APP_NAME":
            assignment = tokens[index + 2:index + 4]
            following = tokens[index + 4:index + 5]
            if (len(assignment) != 2 or assignment[0][0] != "=" or assignment[1].lastgroup != "string"
                    or (following and (following[0][0] in _EXPRESSION_CONTINUATIONS or following[0].lastgroup == "string"))):
                raise PackError(f"APP_NAME must be a static local string literal: {entry}", EXIT_PROJECT)
            try:
                names.append(_string_value(assignment[1][0]))
            except ValueError as error:
                raise PackError(f"Invalid APP_NAME in {entry}: {error}", EXIT_PROJECT) from error
        if word in {"function", "then", "do", "repeat"}:
            depth += 1
    if len(names) != 1 or not names[0].strip():
        raise PackError(f"Define exactly one non-empty local APP_NAME string at the top level of {entry}.", EXIT_PROJECT)
    name = names[0]
    if name == "LudorkSample":
        raise PackError(
            "Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging.",
            EXIT_APP_NAME_UNCHANGED,
        )
    if any(ord(character) < 32 or ord(character) == 127 for character in name):
        raise PackError(f"APP_NAME must not contain control characters: {entry}", EXIT_PROJECT)
    return name


def artifact_name(name: str) -> str:
    normalized = unicodedata.normalize("NFC", name)
    safe = _ARTIFACT_NAME_PATTERN.sub("-", normalized)
    safe = re.sub(r"\s+", " ", safe).strip(" .")
    safe = safe[:_ARTIFACT_NAME_MAX_LENGTH].rstrip(" .") or _ARTIFACT_NAME_FALLBACK
    if pathlib.PureWindowsPath(safe).is_reserved():
        safe = "_" + safe[:_ARTIFACT_NAME_MAX_LENGTH - 1]
    return safe


def prepare_output(project: pathlib.Path, dist: pathlib.Path) -> pathlib.Path:
    project = project.expanduser().resolve()
    dist = dist.expanduser().absolute()
    output = dist / artifact_name(read_app_name(project))
    return prepare_directory(project, output)


def prepare_directory(project: pathlib.Path, output: pathlib.Path) -> pathlib.Path:
    project = project.expanduser().resolve()
    output = output.expanduser().absolute()
    for path in (output, *output.parents):
        if path.is_symlink() or path.is_junction():
            raise PackError(f"Package output path must not contain a link: {path}", EXIT_PROJECT)
    output = output.resolve()
    if output == project or output in project.parents:
        raise PackError(f"Package output overlaps the source project: {output}", EXIT_PROJECT)
    for name in ("Assets", "Data", "Scripts", "Binaries", "EditorCache", "Cache", "bin", "build", "Licenses", "ThirdPartySource", "Engine", "Application", "Intermediate"):
        if output.is_relative_to(project / name):
            raise PackError(f"Package output overlaps project content: {output}", EXIT_PROJECT)
    if output.exists():
        if not output.is_dir():
            raise PackError(f"Package directory conflicts with a file: {output}", EXIT_PROJECT)
        shutil.rmtree(output)
    output.mkdir(parents=True)
    return output
