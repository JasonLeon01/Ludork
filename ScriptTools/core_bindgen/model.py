from __future__ import annotations

"""Data model used by the Core binding generator."""

from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class ParsedType:
    name: str
    arguments: tuple[ParsedType, ...] = ()


@dataclass
class ParameterPlan:
    declaration: str
    argument: str
    prelude: list[str] = field(default_factory=list)


@dataclass
class Member:
    name: str
    declaration: str
    doc: str
    kind: str
    decorators: list[tuple[str, dict[str, str]]] = field(default_factory=list)
    options: dict[str, str] = field(default_factory=dict)
    access: str = "public"
    line: int = 0


@dataclass
class TypeInfo:
    name: str
    bases: list[str]
    doc: str
    source: Path
    options: dict[str, str] = field(default_factory=dict)
    decorators: list[tuple[str, dict[str, str]]] = field(default_factory=list)
    constructors: list[Member] = field(default_factory=list)
    methods: list[Member] = field(default_factory=list)
    properties: list[Member] = field(default_factory=list)
    class_properties: list[Member] = field(default_factory=list)
    injectors: list[Member] = field(default_factory=list)


@dataclass(frozen=True)
class LuaAlternative:
    shape: str
    sources: tuple[str, ...]
    assignments: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class LuaEmit:
    predicates: tuple[tuple[str, str], ...]
    shape: str
    values: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class MacroInvocation:
    kind: str
    arguments: str
    start: int
    end: int
