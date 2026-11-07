from __future__ import annotations

import re


class LuaSyntaxError(ValueError):
    def __init__(self, source: str, position: int, message: str) -> None:
        self.position = position
        self.line = 1
        line_start = 0
        for newline in re.finditer(r"\r\n|\n\r|\r|\n", source[:position]):
            self.line += 1
            line_start = newline.end()
        self.column = position - line_start + 1
        self.message = message
        super().__init__(f"{self.line}:{self.column}: {message}")
