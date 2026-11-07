from __future__ import annotations

from .constants import LUA_NAME_PATTERN, LUA_RESERVED_WORDS
from .literals import lua_string


def is_identifier(value: str) -> bool:
    return LUA_NAME_PATTERN.fullmatch(value) is not None and value not in LUA_RESERVED_WORDS


def lua_key(value: str) -> str:
    return value if is_identifier(value) else f"[{lua_string(value)}]"
