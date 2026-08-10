using System;
using System.Globalization;
using System.Text;

namespace Ludork.Views.Utils;

public static class EditorDisplayName
{
    public static string Format(string name)
    {
        if (string.IsNullOrWhiteSpace(name))
            return name;

        string value = name.Trim();
        StringBuilder result = new(value.Length + 8);
        char previous = '\0';
        bool separated = false;
        for (int index = 0; index < value.Length; index++)
        {
            char current = value[index];
            if (current == '_' || current == '-' || char.IsWhiteSpace(current))
            {
                separated = result.Length > 0;
                continue;
            }

            char next = index + 1 < value.Length ? value[index + 1] : '\0';
            bool wordBoundary = result.Length > 0
                && (separated
                    || char.IsDigit(current) && char.IsLetter(previous)
                    || char.IsUpper(current)
                        && (char.IsLower(previous)
                            || char.IsUpper(previous) && char.IsLower(next)
                            || char.IsDigit(previous) && char.IsLower(next)));
            if (wordBoundary && result[^1] != ' ')
                result.Append(' ');
            char displayed = result.Length == 0 || separated
                ? char.ToUpper(current, CultureInfo.InvariantCulture)
                : current;
            result.Append(displayed);
            previous = current;
            separated = false;
        }

        if (result.Length == 0)
            return value;
        return result.ToString();
    }
}
