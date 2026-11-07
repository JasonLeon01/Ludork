from __future__ import annotations

import re

from .constants import LUA_NAME_PATTERN, LUA_RESERVED_WORDS
from .literals import ASCII_WHITESPACE, LONG_OPEN, NEWLINE, long_bracket_end, scan_string_literal
from .lua_token import LuaToken
from .syntax_error import LuaSyntaxError


_DECIMAL = re.compile(r"(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?")
_HEXADECIMAL = re.compile(r"0[xX](?:[0-9a-fA-F]+(?:\.[0-9a-fA-F]*)?|\.[0-9a-fA-F]+)(?:[pP][+-]?[0-9]+)?")
_HEX_DIGITS = frozenset("0123456789abcdefABCDEF")
_COMPOUND_SYMBOLS = frozenset({"...", "..", "//", "<<", ">>", "==", "~=", "<=", ">=", "::"})
_SYMBOLS = frozenset("+-*/%^#&~|<>=(){}[];:,.")


def _number_end(source: str, start: int) -> int:
    position = start + 1
    hexadecimal = source.startswith(("0x", "0X"), start)
    if hexadecimal:
        position += 1
    exponent = "pP" if hexadecimal else "eE"
    while position < len(source):
        character = source[position]
        if character in exponent:
            position += 1
            if position < len(source) and source[position] in "+-":
                position += 1
        elif character in _HEX_DIGITS or character == ".":
            position += 1
        else:
            break
    if position < len(source) and LUA_NAME_PATTERN.match(source, position) is not None:
        position += 1
    pattern = _HEXADECIMAL if hexadecimal else _DECIMAL
    if pattern.fullmatch(source[start:position]) is None:
        raise LuaSyntaxError(source, start, "Malformed Lua number")
    return position


def tokenize(source: str) -> list[LuaToken]:
    tokens: list[LuaToken] = []
    position = 0
    line = 1
    column = 1
    while position < len(source):
        start = position
        character = source[position]
        kind = None
        string_value = None
        if character in ASCII_WHITESPACE:
            position += 1
            while position < len(source) and source[position] in ASCII_WHITESPACE:
                position += 1
        elif source.startswith("--", position):
            position += 2
            if LONG_OPEN.match(source, position) is not None:
                position, _, _ = long_bracket_end(source, position, comment=True)
            else:
                while position < len(source) and source[position] not in "\r\n":
                    position += 1
        elif character in {'"', "'"} or (character == "[" and LONG_OPEN.match(source, position) is not None):
            position, string_value = scan_string_literal(source, position)
            kind = "string"
        elif character == "[" and source.startswith("[=", position):
            raise LuaSyntaxError(source, position, "Invalid Lua long string delimiter")
        elif (name := LUA_NAME_PATTERN.match(source, position)) is not None:
            position = name.end()
            kind = "keyword" if name[0] in LUA_RESERVED_WORDS else "name"
        elif character in "0123456789" or (character == "." and position + 1 < len(source) and source[position + 1] in "0123456789"):
            position = _number_end(source, position)
            kind = "number"
        elif character in _SYMBOLS:
            width = 3 if source.startswith("...", position) else 2 if source[position:position + 2] in _COMPOUND_SYMBOLS else 1
            position += width
            kind = "symbol"
        else:
            raise LuaSyntaxError(source, position, f"Unexpected character in Lua source: {character!r}")
        if kind is not None:
            tokens.append(LuaToken(kind, source[start:position], start, position, line, column, string_value))
        last_newline = None
        for newline in NEWLINE.finditer(source, start, position):
            line += 1
            last_newline = newline
        column = position - last_newline.end() + 1 if last_newline is not None else column + position - start
    return tokens
