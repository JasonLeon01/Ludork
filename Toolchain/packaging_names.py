from __future__ import annotations

import pathlib
import re
import unicodedata

from .file_replace import remove_tree
from .lua_syntax.lexer import tokenize
from .lua_syntax.syntax_error import LuaSyntaxError
from .pack_error import PackError
from .packaging_constants import (
    EXIT_APP_NAME_UNCHANGED,
    EXIT_PROJECT,
)


_ARTIFACT_NAME_PATTERN = re.compile(r'[\x00-\x1f\x7f<>:"/\\|?*;]+')
_ARTIFACT_NAME_MAX_LENGTH = 80
_ARTIFACT_NAME_FALLBACK = "Ludork Game"
_EXPRESSION_CONTINUATIONS = frozenset({
    "..", "...", "+", "-", "*", "/", "//", "%", "^", "&", "|", "~", "<<", ">>", "<", ">", "=",
    "==", "~=", "<=", ">=", "and", "or", ".", "[", "(", "{", ":", ",",
})


def read_runtime_app_name(project: pathlib.Path) -> str:
    entry = project / "Scripts" / "Entry.lua"
    try:
        with entry.open(encoding="utf-8-sig", newline="") as script:
            source = script.read()
    except (OSError, UnicodeError) as error:
        raise PackError(f"Unable to read Lua entry script {entry}: {error}", EXIT_PROJECT) from error
    try:
        tokens = tokenize(source)
    except LuaSyntaxError as error:
        raise PackError(f"{entry}:{error}", EXIT_PROJECT) from error
    names: list[str] = []
    depth = 0
    table_scopes: list[int] = []
    for index, token in enumerate(tokens):
        if token.kind == "string":
            continue
        word = token.lexeme
        previous = tokens[index - 1].lexeme if index else ""
        if word == "APP_NAME" and previous == "function":
            raise PackError(f"APP_NAME must not be reassigned: {entry}", EXIT_PROJECT)
        in_table_fields = bool(table_scopes) and table_scopes[-1] == depth
        assignment_end = index + 1
        if word == "APP_NAME" and not in_table_fields:
            while (assignment_end + 1 < len(tokens) and tokens[assignment_end].lexeme == ","
                    and tokens[assignment_end + 1].kind == "name"):
                assignment_end += 2
        if (word == "APP_NAME" and assignment_end < len(tokens) and tokens[assignment_end].lexeme == "="
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
        if depth == 0 and word == "local" and index + 1 < len(tokens) and tokens[index + 1].lexeme == "APP_NAME":
            assignment = tokens[index + 2:index + 4]
            following = tokens[index + 4:index + 5]
            if (len(assignment) != 2 or assignment[0].lexeme != "=" or assignment[1].kind != "string"
                    or (following and (following[0].lexeme in _EXPRESSION_CONTINUATIONS or following[0].kind == "string"))):
                raise PackError(f"APP_NAME must be a static local string literal: {entry}", EXIT_PROJECT)
            literal = assignment[1]
            assert literal.string_value is not None
            try:
                names.append(literal.string_value.decode("utf-8"))
            except UnicodeDecodeError as error:
                raise PackError(
                    f"Invalid APP_NAME in {entry}:{literal.line}:{literal.column}: the string must be valid UTF-8",
                    EXIT_PROJECT,
                ) from error
        if word in {"function", "then", "do", "repeat"}:
            depth += 1
    if len(names) != 1:
        raise PackError(f"Define exactly one non-empty local APP_NAME string at the top level of {entry}.", EXIT_PROJECT)
    name = names[0].strip(" \t\r\n\v\f")
    if (not name or name in {".", ".."} or any(character in "/\\:" for character in name)
            or any(ord(character) < 32 or ord(character) == 127 for character in name)):
        raise PackError(
            f"APP_NAME must be a single directory name without path separators, colons or control characters: {entry}",
            EXIT_PROJECT,
        )
    return name


def read_app_name(project: pathlib.Path) -> str:
    name = read_runtime_app_name(project)
    if name == "LudorkSample":
        raise PackError(
            "Change APP_NAME in Scripts/Entry.lua from LudorkSample to a name unique to your game before packaging.",
            EXIT_APP_NAME_UNCHANGED,
        )
    return name


def artifact_name(name: str) -> str:
    normalized = unicodedata.normalize("NFC", name)
    safe = _ARTIFACT_NAME_PATTERN.sub("-", normalized)
    safe = re.sub(r"\s+", " ", safe).strip(" .")
    safe = safe[:_ARTIFACT_NAME_MAX_LENGTH].rstrip(" .") or _ARTIFACT_NAME_FALLBACK
    if pathlib.PureWindowsPath(safe).is_reserved():
        safe = "_" + safe[:_ARTIFACT_NAME_MAX_LENGTH - 1]
    return safe


def prepare_output(project: pathlib.Path, dist: pathlib.Path, package_name: str) -> pathlib.Path:
    project = project.expanduser().resolve()
    dist = dist.expanduser().absolute()
    output = dist / package_name
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
        try:
            remove_tree(output)
        except OSError as error:
            raise PackError(
                f"Unable to replace the existing package directory {output}: {error}",
                EXIT_PROJECT,
            ) from error
    output.mkdir(parents=True)
    return output
