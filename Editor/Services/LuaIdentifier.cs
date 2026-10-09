using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

internal static class LuaIdentifier
{
    private static readonly HashSet<string> keywords = new(StringComparer.Ordinal)
    {
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "global", "goto", "if",
        "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while",
    };

    public static bool IsValid(string value)
    {
        return value.Length != 0 && !keywords.Contains(value)
            && (value[0] == '_' || char.IsLetter(value[0]))
            && value.Skip(1).All(character => character == '_' || char.IsLetterOrDigit(character));
    }
}
