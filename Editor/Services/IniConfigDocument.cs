using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Services;

internal sealed class IniConfigDocument : ConfigDocument
{
    private readonly List<string> lines;
    private readonly string newline;
    private bool trailingNewline;

    private IniConfigDocument(List<string> lines, string newline, bool trailingNewline)
    {
        this.lines = lines;
        this.newline = newline;
        this.trailingNewline = trailingNewline;
    }

    public static IniConfigDocument Parse(string text)
    {
        string newline = text.Contains("\r\n", StringComparison.Ordinal) ? "\r\n" : "\n";
        bool trailingNewline = text.EndsWith("\n", StringComparison.Ordinal)
            || text.EndsWith("\r", StringComparison.Ordinal);
        string normalized = text.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n');
        List<string> lines = normalized.Length == 0
            ? []
            : normalized.Split('\n').ToList();
        if (trailingNewline && lines.Count > 0 && lines[^1].Length == 0)
            lines.RemoveAt(lines.Count - 1);
        return new IniConfigDocument(lines, newline, trailingNewline);
    }

    public override string? GetValue(string section, string key)
    {
        if (!findSection(section, out int headerIndex, out int endIndex))
            return null;
        for (int index = headerIndex + 1; index < endIndex; index += 1)
        {
            if (tryReadKey(lines[index], out string candidate, out int separator)
                && candidate.Equals(key, StringComparison.OrdinalIgnoreCase))
            {
                return lines[index][(separator + 1)..].Trim();
            }
        }
        return null;
    }

    public override void SetValue(string section, string key, string value)
    {
        if (!findSection(section, out int headerIndex, out int endIndex))
        {
            if (lines.Count > 0 && lines[^1].Length != 0)
                lines.Add(string.Empty);
            lines.Add($"[{section}]");
            lines.Add($"{key} = {value}");
            trailingNewline = true;
            return;
        }
        for (int index = headerIndex + 1; index < endIndex; index += 1)
        {
            string line = lines[index];
            if (!tryReadKey(line, out string candidate, out int separator)
                || !candidate.Equals(key, StringComparison.OrdinalIgnoreCase))
                continue;
            int valueIndex = separator + 1;
            while (valueIndex < line.Length && char.IsWhiteSpace(line[valueIndex]))
                valueIndex += 1;
            lines[index] = line[..(separator + 1)] + line[(separator + 1)..valueIndex] + value;
            return;
        }
        lines.Insert(endIndex, $"{key} = {value}");
        trailingNewline = true;
    }

    public override string ToText()
    {
        string result = string.Join(newline, lines);
        return trailingNewline && lines.Count > 0 ? result + newline : result;
    }

    private bool findSection(string section, out int headerIndex, out int endIndex)
    {
        headerIndex = -1;
        endIndex = lines.Count;
        for (int index = 0; index < lines.Count; index += 1)
        {
            string line = lines[index].Trim();
            if (line.Length < 3 || line[0] != '[' || line[^1] != ']')
                continue;
            if (headerIndex >= 0)
            {
                endIndex = index;
                return true;
            }
            string name = line[1..^1].Trim();
            if (name.Equals(section, StringComparison.OrdinalIgnoreCase))
                headerIndex = index;
        }
        return headerIndex >= 0;
    }

    private static bool tryReadKey(string line, out string key, out int separator)
    {
        key = string.Empty;
        separator = -1;
        string trimmed = line.TrimStart();
        if (trimmed.Length == 0 || trimmed[0] is '#' or ';')
            return false;
        int equal = line.IndexOf('=');
        int colon = line.IndexOf(':');
        separator = equal < 0 ? colon : colon < 0 ? equal : Math.Min(equal, colon);
        if (separator < 0)
            return false;
        key = line[..separator].Trim();
        return key.Length != 0;
    }
}
