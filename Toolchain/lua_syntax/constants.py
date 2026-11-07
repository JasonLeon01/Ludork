import re


LUA_NAME_PATTERN = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")
LUA_RESERVED_WORDS = frozenset({
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
    "true", "until", "while",
})
