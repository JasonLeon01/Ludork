# -*- encoding: utf-8 -*-
from __future__ import annotations

import re
import os
import sys
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional

from clang.cindex import Config

_LLVM_PATHS = [
    r"C:\Program Files\LLVM\bin",
    r"C:\Program Files (x86)\LLVM\bin",
    "/usr/lib/llvm-18/lib",
    "/usr/lib/llvm-17/lib",
    "/usr/local/opt/llvm/lib",
]
for _path in _LLVM_PATHS:
    if os.path.isdir(_path):
        Config.set_library_path(_path)
        break

from clang.cindex import Index, Cursor, CursorKind, TranslationUnit


class BindKind(Enum):
    CLASS = auto()
    FUNCTION = auto()
    METHOD = auto()
    PROPERTY = auto()
    INIT = auto()
    IGNORE = auto()


@dataclass
class BindAnnotation:
    kind: BindKind
    docstring: str = ""
    options: dict = field(default_factory=dict)


@dataclass
class ParamInfo:
    name: str
    type_spelling: str
    default_value: Optional[str] = None


@dataclass
class MethodInfo:
    name: str
    params: list[ParamInfo] = field(default_factory=list)
    return_type: str = ""
    is_const: bool = False
    is_virtual: bool = False
    is_static: bool = False
    is_overloaded: bool = False
    annotation: Optional[BindAnnotation] = None


@dataclass
class PropertyInfo:
    name: str
    type_spelling: str
    is_readonly: bool = False
    annotation: Optional[BindAnnotation] = None


@dataclass
class ConstructorInfo:
    params: list[ParamInfo] = field(default_factory=list)
    annotation: Optional[BindAnnotation] = None


@dataclass
class ClassInfo:
    name: str
    bases: list[str] = field(default_factory=list)
    constructors: list[ConstructorInfo] = field(default_factory=list)
    methods: list[MethodInfo] = field(default_factory=list)
    properties: list[PropertyInfo] = field(default_factory=list)
    annotation: Optional[BindAnnotation] = None
    has_deleted_default_ctor: bool = False


@dataclass
class FunctionInfo:
    name: str
    params: list[ParamInfo] = field(default_factory=list)
    return_type: str = ""
    is_overloaded: bool = False
    annotation: Optional[BindAnnotation] = None


@dataclass
class ParsedHeader:
    filepath: str
    classes: list[ClassInfo] = field(default_factory=list)
    functions: list[FunctionInfo] = field(default_factory=list)
    includes: list[str] = field(default_factory=list)


_ANNOTATION_RE = re.compile(r"BIND_(CLASS|FUNCTION|METHOD|PROPERTY|INIT|IGNORE)(?:\(([^)]*)\))?")
_OPTION_RE = re.compile(r'(\w+)\s*=\s*(?:"([^"]*)"|(\w+))')


def _parse_annotation_options(option_str: str) -> dict:
    options = {}
    if not option_str:
        return options
    for match in _OPTION_RE.finditer(option_str):
        key = match.group(1)
        value = match.group(2) if match.group(2) is not None else match.group(3)
        if value.lower() == "true":
            value = True
        elif value.lower() == "false":
            value = False
        options[key] = value
    return options


def _normalize_comment_line(line: str) -> str:
    s = line.strip()

    if s.startswith("//"):
        s = s[2:]
        while s.startswith("/"):
            s = s[1:]
        if s.startswith("<"):
            s = s[1:]
        return s.lstrip()

    if s.startswith("/*"):
        s = s[2:]
        while s.startswith("*") or s.startswith("!"):
            s = s[1:]
        s = s.lstrip()

    if s.endswith("*/"):
        s = s[:-2].rstrip()

    if s.startswith("*"):
        s = s[1:].lstrip()
    if s.startswith("<"):
        s = s[1:].lstrip()

    return s


def _parse_comment_block(raw_comment: str) -> Optional[BindAnnotation]:
    if not raw_comment:
        return None

    lines = raw_comment.strip().splitlines()
    annotation = None
    normalized_lines: list[str] = []

    for line in lines:
        stripped = _normalize_comment_line(line)
        normalized_lines.append(stripped)

        match = _ANNOTATION_RE.search(stripped)
        if match:
            kind_str = match.group(1)
            options_str = match.group(2) or ""
            kind = BindKind[kind_str]
            options = _parse_annotation_options(options_str)
            annotation = BindAnnotation(kind=kind, options=options)

    if annotation is not None:
        docstring_lines = []
        for stripped in normalized_lines:
            if _ANNOTATION_RE.search(stripped):
                continue
            if stripped:
                docstring_lines.append(stripped)
        annotation.docstring = "\n".join(docstring_lines)

    return annotation


def _get_raw_comment(cursor: Cursor) -> str:
    return cursor.raw_comment or ""


_ANNOTATION_MACRO_LINE_RE = re.compile(r"^\s*BIND_(CLASS|FUNCTION|METHOD|PROPERTY|INIT|IGNORE)(?:\([^)]*\))?\s*;?\s*$")


def _is_annotation_macro_line(line: str) -> bool:
    stripped = line.strip()
    if not stripped:
        return False
    if stripped.startswith("#"):
        return False
    if stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*"):
        return False
    return _ANNOTATION_MACRO_LINE_RE.match(line) is not None


def _get_annotation_block_above(cursor: Cursor, source_lines: list[str]) -> str:
    loc = cursor.location
    if not loc or not loc.file:
        return ""

    line_num = loc.line - 1
    if line_num <= 0 or line_num > len(source_lines):
        return ""

    def _is_line_comment(line: str) -> bool:
        return line.strip().startswith("//")

    def _is_block_comment_end(line: str) -> bool:
        return "*/" in line

    def _is_block_comment_start(line: str) -> bool:
        return "/*" in line

    comment_lines: list[str] = []
    i = line_num - 1

    # Skip multiline declaration preamble (e.g. return type on previous line).
    while i >= 0:
        stripped = source_lines[i].strip()
        if not stripped:
            break
        if (
            _is_line_comment(source_lines[i])
            or _is_block_comment_end(source_lines[i])
            or _is_annotation_macro_line(source_lines[i])
        ):
            break
        if ";" in stripped or "{" in stripped or "}" in stripped:
            return ""
        i -= 1

    while i >= 0:
        stripped = source_lines[i].strip()

        if not stripped:
            break

        if _is_annotation_macro_line(source_lines[i]):
            comment_lines.insert(0, source_lines[i])
            i -= 1
            continue

        if _is_line_comment(source_lines[i]):
            comment_lines.insert(0, source_lines[i])
            i -= 1
            continue

        if _is_block_comment_end(source_lines[i]):
            comment_lines.insert(0, source_lines[i])
            i -= 1
            while i >= 0:
                comment_lines.insert(0, source_lines[i])
                if _is_block_comment_start(source_lines[i]):
                    i -= 1
                    break
                i -= 1
            continue

        break

    return "\n".join(comment_lines)


def _parse_default_value(cursor: Cursor, source_lines: list[str]) -> Optional[str]:
    children = list(cursor.get_children())
    if not children:
        return None

    extent = cursor.extent
    if not extent or not extent.start.file:
        return None

    start_line = extent.start.line - 1
    start_col = extent.start.column - 1
    end_line = extent.end.line - 1
    end_col = extent.end.column - 1

    if start_line == end_line:
        text = source_lines[start_line][start_col:end_col]
    else:
        lines_text = [source_lines[start_line][start_col:]]
        for l in range(start_line + 1, end_line):
            lines_text.append(source_lines[l])
        lines_text.append(source_lines[end_line][:end_col])
        text = "\n".join(lines_text)

    eq_idx = text.find("=")
    if eq_idx >= 0:
        return text[eq_idx + 1 :].strip()
    return None


def _parse_params(cursor: Cursor, source_lines: list[str]) -> list[ParamInfo]:
    params = []
    for child in cursor.get_children():
        if child.kind == CursorKind.PARM_DECL:
            default_val = _parse_default_value(child, source_lines)
            params.append(
                ParamInfo(
                    name=child.spelling or f"arg{len(params)}",
                    type_spelling=child.type.spelling,
                    default_value=default_val,
                )
            )
    return params


class HeaderParser:
    def __init__(self, include_paths: list[str] = None, extra_args: list[str] = None):
        self.index = Index.create()
        self.include_paths = include_paths or []
        self.extra_args = extra_args or []

    def parse(self, filepath: str) -> ParsedHeader:
        args = ["-x", "c++", "-std=c++17", "-DBINDGEN_PARSING"]
        for inc in self.include_paths:
            args.append(f"-I{inc}")
        args.extend(self.extra_args)

        tu = self.index.parse(
            filepath,
            args=args,
            options=(
                TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD
                | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
                | TranslationUnit.PARSE_INCOMPLETE
            ),
        )

        with open(filepath, "r", encoding="utf-8") as f:
            source_lines = f.read().splitlines()

        result = ParsedHeader(filepath=filepath)
        self._collect_includes(tu.cursor, filepath, result)
        self._visit(tu.cursor, filepath, source_lines, result)
        self._mark_overloads(result)
        return result

    def _collect_includes(self, cursor: Cursor, filepath: str, result: ParsedHeader):
        for child in cursor.get_children():
            if child.kind == CursorKind.INCLUSION_DIRECTIVE:
                if child.location.file and os.path.normpath(child.location.file.name) == os.path.normpath(filepath):
                    result.includes.append(child.displayname)

    def _visit(self, cursor: Cursor, filepath: str, source_lines: list[str], result: ParsedHeader):
        for child in cursor.get_children():
            if child.location.file and os.path.normpath(child.location.file.name) != os.path.normpath(filepath):
                continue

            if child.kind in (CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL):
                class_info = self._parse_class(child, source_lines)
                if class_info:
                    result.classes.append(class_info)
            elif child.kind == CursorKind.FUNCTION_DECL:
                func_info = self._parse_function(child, source_lines)
                if func_info:
                    result.functions.append(func_info)
            elif child.kind == CursorKind.NAMESPACE:
                self._visit(child, filepath, source_lines, result)

    def _parse_class(self, cursor: Cursor, source_lines: list[str]) -> Optional[ClassInfo]:
        if not cursor.is_definition():
            return None

        comment = _get_annotation_block_above(cursor, source_lines)
        annotation = _parse_comment_block(comment)

        if annotation is None or annotation.kind != BindKind.CLASS:
            return None

        class_info = ClassInfo(name=cursor.spelling, annotation=annotation)

        for child in cursor.get_children():
            if child.kind == CursorKind.CXX_BASE_SPECIFIER:
                tokens = [t.spelling for t in child.get_tokens()]
                base_tokens = [t for t in tokens if t not in ("public", "protected", "private", "virtual", ":", ",")]
                if base_tokens:
                    base_name = "".join(base_tokens)
                    class_info.bases.append(base_name)

        is_struct = cursor.kind == CursorKind.STRUCT_DECL
        current_access = "public" if is_struct else "private"

        for child in cursor.get_children():
            if child.kind == CursorKind.CXX_ACCESS_SPEC_DECL:
                tokens = list(child.get_tokens())
                if tokens:
                    token_text = tokens[0].spelling
                    if token_text in ("public", "protected", "private"):
                        current_access = token_text
                continue

            if current_access != "public":
                continue

            member_comment = _get_annotation_block_above(child, source_lines)
            member_annotation = _parse_comment_block(member_comment)

            if member_annotation and member_annotation.kind == BindKind.IGNORE:
                continue

            if child.kind == CursorKind.CONSTRUCTOR:
                if child.is_deleted_method():
                    class_info.has_deleted_default_ctor = True
                    continue
                ctor = ConstructorInfo(
                    params=_parse_params(child, source_lines),
                    annotation=member_annotation,
                )
                if member_annotation is None or member_annotation.kind == BindKind.INIT:
                    class_info.constructors.append(ctor)

            elif child.kind == CursorKind.CXX_METHOD:
                if child.is_deleted_method():
                    continue
                if child.spelling.startswith("~") or child.spelling.startswith("operator"):
                    continue

                method = MethodInfo(
                    name=child.spelling,
                    params=_parse_params(child, source_lines),
                    return_type=child.result_type.spelling,
                    is_const=child.is_const_method(),
                    is_virtual=child.is_virtual_method(),
                    is_static=child.is_static_method(),
                    annotation=member_annotation,
                )
                if member_annotation is None or member_annotation.kind == BindKind.METHOD:
                    class_info.methods.append(method)

            elif child.kind == CursorKind.FIELD_DECL:
                is_readonly = False
                if member_annotation and member_annotation.options.get("readonly"):
                    is_readonly = True

                prop = PropertyInfo(
                    name=child.spelling,
                    type_spelling=child.type.spelling,
                    is_readonly=is_readonly,
                    annotation=member_annotation,
                )
                if member_annotation is None or member_annotation.kind == BindKind.PROPERTY:
                    class_info.properties.append(prop)

        return class_info

    def _parse_function(self, cursor: Cursor, source_lines: list[str]) -> Optional[FunctionInfo]:
        comment = _get_annotation_block_above(cursor, source_lines)
        annotation = _parse_comment_block(comment)

        if annotation is None or annotation.kind != BindKind.FUNCTION:
            return None

        return FunctionInfo(
            name=cursor.spelling,
            params=_parse_params(cursor, source_lines),
            return_type=cursor.result_type.spelling,
            annotation=annotation,
        )

    def _mark_overloads(self, result: ParsedHeader):
        name_count: dict[str, int] = {}
        for func in result.functions:
            name_count[func.name] = name_count.get(func.name, 0) + 1
        for func in result.functions:
            if name_count[func.name] > 1:
                func.is_overloaded = True

        for cls in result.classes:
            method_count: dict[str, int] = {}
            for method in cls.methods:
                method_count[method.name] = method_count.get(method.name, 0) + 1
            for method in cls.methods:
                if method_count[method.name] > 1:
                    method.is_overloaded = True
