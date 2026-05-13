# -*- encoding: utf-8 -*-
# pyright: reportAttributeAccessIssue=false

from __future__ import annotations

import re
import os
import sys
import subprocess
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
    "/opt/homebrew/opt/llvm/lib",
]
for _path in _LLVM_PATHS:
    if os.path.isdir(_path):
        Config.set_library_path(_path)
        break

from clang.cindex import Index, Cursor, CursorKind, TranslationUnit


def _argv_has_option(argv: list[str], option: str) -> bool:
    """Return True if ``argv`` contains ``option`` or ``option=value``."""
    for a in argv:
        if a == option or a.startswith(option + "="):
            return True
    return False


def _darwin_clang_implicit_args(extra_args: list[str]) -> list[str]:
    if sys.platform != "darwin":
        return []
    implicit: list[str] = []
    if not _argv_has_option(extra_args, "-isysroot"):
        try:
            proc = subprocess.run(
                ["xcrun", "--show-sdk-path"],
                check=True,
                capture_output=True,
                text=True,
                timeout=30,
            )
            sdk = (proc.stdout or "").strip()
            if sdk and os.path.isdir(sdk):
                implicit.extend(["-isysroot", sdk])
        except (FileNotFoundError, subprocess.CalledProcessError, OSError):
            pass
    if not _argv_has_option(extra_args, "-resource-dir"):
        try:
            proc = subprocess.run(
                ["xcrun", "clang", "-print-resource-dir"],
                check=True,
                capture_output=True,
                text=True,
                timeout=30,
            )
            rd = (proc.stdout or "").strip()
            if rd and os.path.isdir(rd):
                implicit.extend(["-resource-dir", rd])
        except (FileNotFoundError, subprocess.CalledProcessError, OSError):
            pass
    return implicit


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


_ANNOTATION_MACRO_LINE_RE = re.compile(r"^\s*BIND_(CLASS|FUNCTION|METHOD|PROPERTY|INIT|IGNORE)(?:\([^)]*\))?\s*;?\s*$")

_MACRO_BIND_KIND = {
    "BIND_CLASS": BindKind.CLASS,
    "BIND_FUNCTION": BindKind.FUNCTION,
    "BIND_METHOD": BindKind.METHOD,
    "BIND_PROPERTY": BindKind.PROPERTY,
    "BIND_INIT": BindKind.INIT,
    "BIND_IGNORE": BindKind.IGNORE,
}


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


def _find_param_name_in_type(raw: str) -> int:
    depth = 0
    for i in range(len(raw) - 1, -1, -1):
        ch = raw[i]
        if ch in (")", ">", "}", "]"):
            depth += 1
        elif ch in ("(", "<", "{", "["):
            depth -= 1
        elif ch.isspace() and depth == 0:
            return i + 1
    return 0


def _split_params_by_comma(text: str) -> list[str]:
    parts = []
    depth = 0
    current = []
    for ch in text:
        if ch in ("<", "(", "{", "["):
            depth += 1
            current.append(ch)
        elif ch in (">", ")", "}", "]"):
            depth -= 1
            current.append(ch)
        elif ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    remainder = "".join(current).strip()
    if remainder:
        parts.append(remainder)
    return parts


def _parse_param_decl_text_list(params_text: str) -> list[ParamInfo]:
    """Parse a comma-separated C++ parameter list (inside parentheses) into ParamInfo."""
    params_text = params_text.strip()
    if not params_text:
        return []

    raw_params = _split_params_by_comma(params_text)
    result = []
    for idx, raw in enumerate(raw_params):
        raw = raw.strip()
        if not raw:
            continue

        default_val = None
        eq_pos = raw.find("=")
        if eq_pos >= 0:
            default_val = raw[eq_pos + 1 :].strip()
            raw = raw[:eq_pos].strip()

        name_start = _find_param_name_in_type(raw)
        if name_start < len(raw):
            name = raw[name_start:].strip()
            type_spelling = raw[:name_start].strip()
            while name and name[0] in ("*", "&"):
                type_spelling = (type_spelling + " " + name[0]).rstrip()
                name = name[1:].strip()
        else:
            name = f"arg{idx}"
            type_spelling = raw.strip()

        result.append(
            ParamInfo(
                name=name,
                type_spelling=type_spelling,
                default_value=default_val,
            )
        )
    return result


def _parse_params_from_source(cursor: Cursor, source_lines: list[str]) -> list[ParamInfo]:
    extent = cursor.extent
    if not extent:
        return []

    start_line = extent.start.line - 1
    start_col = extent.start.column - 1
    end_line = extent.end.line - 1
    end_col = extent.end.column - 1

    if start_line < 0 or start_line >= len(source_lines):
        return []

    if start_line == end_line:
        text = source_lines[start_line][start_col:end_col]
    else:
        text = source_lines[start_line][start_col:]
        for i in range(start_line + 1, end_line):
            text += " " + source_lines[i]
        text += source_lines[end_line][:end_col]

    paren_start = text.find("(")
    paren_end = text.rfind(")")
    if paren_start < 0 or paren_end < 0 or paren_start >= paren_end:
        return []

    params_text = text[paren_start + 1 : paren_end].strip()
    return _parse_param_decl_text_list(params_text)


def _get_param_type_spelling(cursor: Cursor, source_lines: list[str]) -> str:
    if not cursor.extent:
        return cursor.type.spelling

    start_line = cursor.extent.start.line - 1
    start_col = cursor.extent.start.column - 1
    end_line = cursor.extent.end.line - 1
    end_col = cursor.extent.end.column - 1

    if start_line < 0 or start_line >= len(source_lines):
        return cursor.type.spelling

    if start_line == end_line:
        text = source_lines[start_line][start_col:end_col]
    else:
        text = source_lines[start_line][start_col:]
        for i in range(start_line + 1, end_line):
            text += source_lines[i]
        text += source_lines[end_line][:end_col]

    eq_pos = text.find("=")
    if eq_pos >= 0:
        text = text[:eq_pos].rstrip()

    name = cursor.spelling
    if name and text.rstrip().endswith(name):
        text = text[: text.rstrip().rfind(name)].rstrip()

    return text.strip()


def _parse_params(cursor: Cursor, source_lines: list[str]) -> list[ParamInfo]:
    source_params = _parse_params_from_source(cursor, source_lines)
    if source_params:
        return source_params

    params = []
    for child in cursor.get_children():
        if child.kind == CursorKind.PARM_DECL:
            default_val = _parse_default_value(child, source_lines)
            params.append(
                ParamInfo(
                    name=child.spelling or f"arg{len(params)}",
                    type_spelling=_get_param_type_spelling(child, source_lines),
                    default_value=default_val,
                )
            )
    return params


class HeaderParser:
    def __init__(self, include_paths: Optional[list[str]] = None, extra_args: Optional[list[str]] = None):
        self.index = Index.create()
        self.include_paths = include_paths or []
        self.extra_args = extra_args or []

    def parse(self, filepath: str) -> ParsedHeader:
        args = ["-x", "c++", "-std=c++17", "-DBINDGEN_PARSING"]
        for inc in self.include_paths:
            args.append(f"-I{inc}")
        args.extend(_darwin_clang_implicit_args(self.extra_args))
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
        self._fallback_func_names: set[str] = set()
        self._fallback_class_names: set[str] = set()
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
            elif child.kind == CursorKind.MACRO_INSTANTIATION:
                self._handle_macro_instantiation(child, source_lines, result)

    def _handle_macro_instantiation(self, cursor: Cursor, source_lines: list[str], result: ParsedHeader):
        """Handle BIND_* macro instantiations when libclang couldn't parse the
        associated declaration (e.g. complex template return types)."""
        if not cursor.location or not cursor.location.file:
            return
        macro_name = cursor.spelling
        if macro_name not in _MACRO_BIND_KIND:
            return
        kind = _MACRO_BIND_KIND[macro_name]
        if kind == BindKind.IGNORE:
            return

        macro_line_idx = cursor.location.line - 1
        if kind == BindKind.FUNCTION:
            func_info = self._extract_function_from_source(source_lines, macro_line_idx, macro_name)
            if func_info:
                result.functions.append(func_info)
                self._fallback_func_names.add(func_info.name)

    def _extract_function_from_source(self, source_lines: list[str], macro_line_idx: int, macro_name: str):
        """Extract function info from source lines when libclang couldn't parse."""
        import re as _re

        # Build annotation from comment block above macro + macro line itself
        comment_lines = []
        j = macro_line_idx - 1
        while j >= 0:
            stripped = source_lines[j].strip()
            if not stripped:
                break
            if (stripped.startswith('//') or stripped.startswith('/*')
                    or stripped.startswith('*') or '*/' in stripped):
                comment_lines.insert(0, source_lines[j])
                j -= 1
            else:
                break
        comment_lines.append(source_lines[macro_line_idx])
        comment_block = '\n'.join(comment_lines)
        annotation = _parse_comment_block(comment_block)
        if annotation is None or annotation.kind != BindKind.FUNCTION:
            return None

        # Scan forward for function declaration
        i = macro_line_idx + 1
        while i < len(source_lines) and not source_lines[i].strip():
            i += 1
        if i >= len(source_lines):
            return None

        decl_text = ""
        paren_depth = 0
        found_open = False
        while i < len(source_lines):
            line = source_lines[i]
            comment_pos = line.find('//')
            if comment_pos >= 0:
                line = line[:comment_pos]
            decl_text += line

            for ch in line:
                if ch == '(':
                    paren_depth += 1
                    found_open = True
                elif ch == ')':
                    paren_depth -= 1
                    if paren_depth == 0 and found_open:
                        break

            if paren_depth == 0 and found_open:
                break
            i += 1

        # Extract function name (last identifier before '(')
        func_match = _re.search(r'\b([a-zA-Z_]\w*)\s*\(', decl_text)
        if not func_match:
            return None
        func_name = func_match.group(1)

        # Return type is everything before the function name
        name_pos = decl_text.index(func_name)
        return_type = decl_text[:name_pos].strip()

        # Extract parameters
        paren_start = decl_text.index('(')
        paren_end = decl_text.rindex(')')
        params_text = decl_text[paren_start + 1:paren_end]

        param_infos = _parse_param_decl_text_list(params_text)

        return FunctionInfo(
            name=func_name,
            params=param_infos,
            return_type=return_type,
            annotation=annotation,
        )

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

        # Skip if this function was already added via MACRO_INSTANTIATION fallback
        if hasattr(self, '_fallback_func_names') and cursor.spelling in self._fallback_func_names:
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
