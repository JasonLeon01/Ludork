from __future__ import annotations

import bisect
import re
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING

from .model import EnumInfo, TypeAlias, TypeInfo

if TYPE_CHECKING:
    from .context import GeneratorContext


CPP_NAME = r"(?:::)?[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*"


def code_mask(text: str) -> str:
    pattern = re.compile(
        r'//[^\n]*|/\*.*?\*/|(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(.*?\)\1"'
        r'|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"'
        r"|(?:u8|u|U|L)?'(?:\\.|[^'\\])*'",
        re.DOTALL,
    )
    return pattern.sub(
        lambda match: "".join("\n" if char == "\n" else " " for char in match[0]),
        text,
    )


@dataclass
class NativeDeclaration:
    name: str
    cpp_name: str
    cpp_scope: tuple[str, ...]
    kind: str
    start: int
    opening: int | None
    closing: int | None
    access: str
    source: Path
    line: int


class HeaderScopes:
    def __init__(self, path: Path, text: str) -> None:
        self.path = path
        self.text = text
        self.mask = code_mask(text)
        self.parents: dict[int, int | None] = {}
        self.closing: dict[int, int] = {}
        self.braces: list[int] = []
        self.brace_owners: list[int | None] = []
        stack: list[int] = []
        for offset, char in enumerate(self.mask):
            if char == "{":
                self.parents[offset] = stack[-1] if stack else None
                stack.append(offset)
            elif char == "}" and stack:
                self.closing[stack.pop()] = offset
            else:
                continue
            self.braces.append(offset)
            self.brace_owners.append(stack[-1] if stack else None)
        self.declarations: list[NativeDeclaration] = []
        self.by_opening: dict[int, NativeDeclaration] = {}
        pattern = re.compile(
            rf"\b(?P<kind>enum\s+(?:class|struct)|enum|class|struct|namespace)\s*"
            rf"(?:(?:[A-Z][A-Z0-9_]*_API)\s+)?"
            rf"(?P<name>{CPP_NAME})?\s*(?:final\s*)?(?=[:{{;])"
        )
        for match in pattern.finditer(self.mask):
            if re.search(r"\bfriend\s*$", self.mask[max(0, match.start() - 32) : match.start()]):
                continue
            kind = " ".join(match["kind"].split())
            name = match["name"] or ""
            if not name and kind != "namespace":
                continue
            parent = self.direct_scope(match.start())
            enclosing = self.enclosing_brace(match.start())
            if enclosing is not None and parent is None:
                continue
            scope = tuple(parent.cpp_name.split("::")) if parent else ()
            if name.startswith("::"):
                cpp_name = name[2:]
            else:
                cpp_name = "::".join((*scope, name))
                if "::" in name:
                    known = {item.cpp_name for item in self.declarations}
                    qualifier = name.split("::", 1)[0]
                    for length in range(len(scope), -1, -1):
                        if "::".join((*scope[:length], qualifier)) in known:
                            cpp_name = "::".join((*scope[:length], name))
                            break
            if not name:
                cpp_name = "::".join((*scope, f"<anonymous@{match.start()}>"))
            scope = tuple(cpp_name.split("::")[:-1])
            opening = self.mask.find("{", match.end())
            semicolon = self.mask.find(";", match.end())
            if opening < 0 or 0 <= semicolon < opening:
                opening = None
            closing = self.closing.get(opening) if opening is not None else None
            declaration = NativeDeclaration(
                cpp_name.rsplit("::", 1)[-1],
                cpp_name,
                scope,
                kind,
                match.start(),
                opening,
                closing,
                self.access_at(parent, match.start()),
                path,
                text.count("\n", 0, match.start()) + 1,
            )
            self.declarations.append(declaration)
            if opening is not None:
                self.by_opening[opening] = declaration

    def enclosing_brace(self, offset: int) -> int | None:
        index = bisect.bisect_left(self.braces, offset) - 1
        return self.brace_owners[index] if index >= 0 else None

    def direct_scope(self, offset: int) -> NativeDeclaration | None:
        opening = self.enclosing_brace(offset)
        return self.by_opening.get(opening) if opening is not None else None

    def scope_at(self, offset: int) -> tuple[str, ...]:
        declaration = self.direct_scope(offset)
        return tuple(declaration.cpp_name.split("::")) if declaration else ()

    def access_at(self, owner: NativeDeclaration | None, offset: int) -> str:
        if owner is None or owner.kind == "namespace":
            return "public"
        result = "private" if owner.kind == "class" else "public"
        if owner.opening is None:
            return result
        for match in re.finditer(
            r"\b(public|protected|private)\s*:",
            self.mask[owner.opening + 1 : offset],
        ):
            position = owner.opening + 1 + match.start()
            if self.enclosing_brace(position) == owner.opening:
                result = match[1]
        return result

    def declaration_after(self, offset: int) -> NativeDeclaration:
        for declaration in self.declarations:
            if declaration.start >= offset:
                if self.mask[offset : declaration.start].strip():
                    break
                if declaration.opening is None or declaration.closing is None:
                    raise ValueError(
                        f"{self.path}:{declaration.line}: binding requires a complete "
                        f"definition of {declaration.cpp_name}"
                    )
                return declaration
        line = self.text.count("\n", 0, offset) + 1
        raise ValueError(f"{self.path}:{line}: unsupported bound type declaration")

    def aliases(self) -> dict[str, TypeAlias]:
        result: dict[str, TypeAlias] = {}
        patterns = (
            re.compile(r"\busing\s+(?P<name>[A-Za-z_]\w*)\s*=\s*(?P<target>[^;]+);"),
            re.compile(r"\btypedef\s+(?P<target>[^;]+?)\s+(?P<name>[A-Za-z_]\w*)\s*;"),
        )
        for pattern in patterns:
            for match in pattern.finditer(self.mask):
                owner = self.direct_scope(match.start())
                if self.enclosing_brace(match.start()) is not None and owner is None:
                    continue
                scope = self.scope_at(match.start())
                name = "::".join((*scope, match["name"]))
                result[name] = TypeAlias(
                    " ".join(self.text[match.start("target") : match.end("target")].split()),
                    scope,
                    self.path,
                    self.text.count("\n", 0, match.start()) + 1,
                )
        return result

    def direct_body(self, owner: NativeDeclaration) -> str:
        start = owner.opening + 1
        result = list(self.text[start : owner.closing])
        for opening, parent in self.parents.items():
            if parent != owner.opening:
                continue
            for offset in range(opening + 1, self.closing[opening]):
                if result[offset - start] != "\n":
                    result[offset - start] = " "
        return "".join(result)


def register_headers(context: GeneratorContext, paths: list[Path]) -> None:
    for path in dict.fromkeys(paths):
        if path in context.header_scopes:
            continue
        scopes = HeaderScopes(path, path.read_text(encoding="utf-8"))
        context.header_scopes[path] = scopes
        for declaration in scopes.declarations:
            context.native_declarations.setdefault(declaration.cpp_name, []).append(declaration)
        for name, alias in scopes.aliases().items():
            previous = context.type_aliases.get(name)
            if previous is not None and (
                previous.target != alias.target or previous.cpp_scope != alias.cpp_scope
            ):
                raise ValueError(
                    f"{alias.source}:{alias.line}: conflicting C++ alias {name}; "
                    f"first declared at {previous.source}:{previous.line}"
                )
            context.type_aliases[name] = alias


def validate_public_type(context: GeneratorContext, declaration: NativeDeclaration) -> None:
    parts = declaration.cpp_name.split("::")
    for length in range(1, len(parts) + 1):
        name = "::".join(parts[:length])
        declarations = context.native_declarations.get(name, [])
        if name.startswith("<anonymous") or "::<anonymous" in name:
            raise ValueError(
                f"{declaration.source}:{declaration.line}: bound type "
                f"{declaration.cpp_name} is in an anonymous namespace"
            )
        if any(item.access != "public" for item in declarations):
            raise ValueError(
                f"{declaration.source}:{declaration.line}: bound type "
                f"{declaration.cpp_name} must be publicly accessible ({name})"
            )


def binding_identifier(cpp_name: str) -> str:
    parts = cpp_name.split("::")
    if any(re.fullmatch(r"[A-Za-z_]\w*", part) is None for part in parts):
        raise ValueError(f"invalid binding C++ type name: {cpp_name}")
    return parts[0] if len(parts) == 1 else "N" + "".join(
        str(len(part)) + part for part in parts
    )


def validate_bound_types(types: list[TypeInfo | EnumInfo]) -> None:
    declarations: dict[str, TypeInfo | EnumInfo] = {}
    for info in types:
        previous = declarations.get(info.cpp_name)
        if previous is not None:
            raise ValueError(
                f"{info.source}:{info.line}: duplicate bound C++ type {info.cpp_name}; "
                f"first declared at {previous.source}:{previous.line}"
            )
        declarations[info.cpp_name] = info
