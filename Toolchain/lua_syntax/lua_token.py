from __future__ import annotations

from dataclasses import dataclass
from typing import Literal


@dataclass(frozen=True)
class LuaToken:
    kind: Literal["name", "keyword", "number", "string", "symbol"]
    lexeme: str
    start: int
    end: int
    line: int
    column: int
    string_value: bytes | None = None
