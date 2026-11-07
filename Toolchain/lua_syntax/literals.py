from __future__ import annotations

import re

from .syntax_error import LuaSyntaxError


ASCII_WHITESPACE = " \t\r\n\v\f"
NEWLINE = re.compile(r"\r\n|\n\r|\r|\n")
LONG_OPEN = re.compile(r"\[(=*)\[")
_HEX_DIGITS = frozenset("0123456789abcdefABCDEF")
_ESCAPES = dict(zip("abfnrtv\\\"'", b"\a\b\f\n\r\t\v\\\"'"))
_OUTPUT_ESCAPES = {
    "\\": "\\\\", '"': '\\"', "\b": "\\b", "\f": "\\f",
    "\n": "\\n", "\r": "\\r", "\t": "\\t",
}


def long_bracket_end(source: str, start: int, *, comment: bool = False) -> tuple[int, int, int]:
    opening = LONG_OPEN.match(source, start)
    if opening is None:
        raise LuaSyntaxError(source, start, "Invalid Lua long string delimiter")
    closing = "]" + opening[1] + "]"
    content_end = source.find(closing, opening.end())
    if content_end < 0:
        kind = "comment" if comment else "string"
        raise LuaSyntaxError(source, start, f"Unterminated Lua long {kind}")
    return content_end + len(closing), opening.end(), content_end


def _utf8_bytes(codepoint: int) -> bytes:
    if codepoint < 0x80:
        return bytes([codepoint])
    tail: list[int] = []
    first_limit = 0x3F
    while True:
        tail.append(0x80 | (codepoint & 0x3F))
        codepoint >>= 6
        first_limit >>= 1
        if codepoint <= first_limit:
            break
    return bytes([((~first_limit << 1) | codepoint) & 0xFF, *reversed(tail)])


def scan_string_literal(source: str, start: int) -> tuple[int, bytes]:
    if start >= len(source):
        raise LuaSyntaxError(source, start, "Expected a Lua string literal")
    if source[start] == "[":
        end, content_start, content_end = long_bracket_end(source, start)
        newline = NEWLINE.match(source, content_start)
        if newline is not None:
            content_start = newline.end()
        return end, NEWLINE.sub("\n", source[content_start:content_end]).encode("utf-8")
    quote = source[start]
    if quote not in {'"', "'"}:
        raise LuaSyntaxError(source, start, "Expected a Lua string literal")
    position = start + 1
    value = bytearray()
    while position < len(source):
        character = source[position]
        if character == quote:
            return position + 1, bytes(value)
        if character in "\r\n":
            raise LuaSyntaxError(source, position, "Unescaped newline in Lua string")
        if character != "\\":
            value.extend(character.encode("utf-8"))
            position += 1
            continue
        escape_start = position
        position += 1
        if position == len(source):
            break
        escape = source[position]
        position += 1
        if escape in _ESCAPES:
            value.append(_ESCAPES[escape])
        elif escape in "\r\n":
            newline = NEWLINE.match(source, position - 1)
            assert newline is not None
            position = newline.end()
            value.append(10)
        elif escape == "z":
            while position < len(source) and source[position] in ASCII_WHITESPACE:
                position += 1
        elif escape == "x":
            digits = source[position:position + 2]
            if len(digits) != 2 or any(digit not in _HEX_DIGITS for digit in digits):
                raise LuaSyntaxError(source, escape_start, "Invalid hexadecimal escape in Lua string")
            value.append(int(digits, 16))
            position += 2
        elif escape == "u":
            if position == len(source) or source[position] != "{":
                raise LuaSyntaxError(source, escape_start, "Expected '{' in Lua UTF-8 escape")
            position += 1
            digits_start = position
            codepoint = 0
            while position < len(source) and source[position] in _HEX_DIGITS:
                codepoint = (codepoint << 4) | int(source[position], 16)
                if codepoint > 0x7FFFFFFF:
                    raise LuaSyntaxError(source, escape_start, "Lua UTF-8 escape is too large")
                position += 1
            if position == digits_start or position == len(source) or source[position] != "}":
                raise LuaSyntaxError(source, escape_start, "Invalid Lua UTF-8 escape")
            value.extend(_utf8_bytes(codepoint))
            position += 1
        elif escape in "0123456789":
            digits_start = position - 1
            while position < len(source) and position < digits_start + 3 and source[position] in "0123456789":
                position += 1
            byte = int(source[digits_start:position])
            if byte > 255:
                raise LuaSyntaxError(source, escape_start, "Decimal escape in Lua string exceeds 255")
            value.append(byte)
        else:
            raise LuaSyntaxError(source, escape_start, f"Invalid Lua escape \\{escape}")
    raise LuaSyntaxError(source, start, "Unterminated Lua string")


def decode_string_literal(literal: str) -> bytes:
    end, value = scan_string_literal(literal, 0)
    if end != len(literal):
        raise LuaSyntaxError(literal, end, "Unexpected content after Lua string literal")
    return value


def lua_string(value: str) -> str:
    encoded: list[str] = []
    for character in value:
        if character in _OUTPUT_ESCAPES:
            encoded.append(_OUTPUT_ESCAPES[character])
        elif ord(character) < 32 or ord(character) == 127:
            encoded.append(f"\\{ord(character):03d}")
        else:
            encoded.append(character)
    return '"' + "".join(encoded) + '"'
