using Avalonia.Media;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Ludork.Plugin.Avalonia;

public static class MarkdownSyntax
{
    public enum Profile
    {
        Documentation,
        Assistant,
    }

    public static string[] GetLines(string markdown)
    {
        return markdown.Replace("\r\n", "\n", StringComparison.Ordinal).Replace('\r', '\n').Split('\n');
    }

    public static MatchCollection GetInlineMatches(string text, Profile profile)
    {
        return Regex.Matches(text, profile == Profile.Documentation
            ? @"`[^`\n]+`|\*\*[^*\n]+?\*\*|__[^_\n]+?__|~~[^~\n]+?~~|(?<!\*)\*[^*\n]+?\*(?!\*)|!?\[[^\]]*\]\([^)]+\)"
            : @"`[^`\n]+`|\*\*[^*\n]+?\*\*|__[^_\n]+?__|(?<!\*)\*[^*\n]+?\*(?!\*)|\[[^\]]+\]\([^)]+\)");
    }

    public static MarkdownTable? ReadTable(IReadOnlyList<string> lines, int startIndex, Profile profile)
    {
        if (startIndex + 1 >= lines.Count)
            return null;
        string headerLine = lines[startIndex];
        string delimiterLine = lines[startIndex + 1];
        if (profile == Profile.Assistant && !containsTablePipe(headerLine) && !containsTablePipe(delimiterLine))
            return null;
        IReadOnlyList<string> headers = splitTableRow(headerLine, profile);
        IReadOnlyList<string> delimiters = splitTableRow(delimiterLine, profile);
        if (headers.Count == 0 || delimiters.Count != headers.Count
            || delimiters.Any(value => !Regex.IsMatch(value, "^:?-{3,}:?$")))
            return null;
        TextAlignment[] alignments = delimiters.Select(value => value.StartsWith(':') && value.EndsWith(':')
            ? TextAlignment.Center
            : value.EndsWith(':') ? TextAlignment.Right : TextAlignment.Left).ToArray();
        List<IReadOnlyList<string>> rows = [];
        int index = startIndex + 2;
        while (index < lines.Count && !string.IsNullOrWhiteSpace(lines[index])
            && (profile == Profile.Documentation ? lines[index].Contains('|') : containsTablePipe(lines[index])))
        {
            List<string> row = splitTableRow(lines[index], profile);
            if (row.Count > headers.Count)
                row.RemoveRange(headers.Count, row.Count - headers.Count);
            while (row.Count < headers.Count)
                row.Add(string.Empty);
            rows.Add(row);
            index++;
        }
        return new MarkdownTable(headers, alignments, rows, index - startIndex);
    }

    private static bool containsTablePipe(string line)
    {
        bool escaped = false;
        bool inCode = false;
        foreach (char character in line)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (character == '\\')
            {
                escaped = true;
                continue;
            }
            if (character == '`')
            {
                inCode = !inCode;
                continue;
            }
            if (character == '|' && !inCode)
                return true;
        }
        return false;
    }

    private static List<string> splitTableRow(string line, Profile profile)
    {
        string value = line.Trim();
        if (profile == Profile.Documentation)
        {
            if (value.StartsWith('|'))
                value = value[1..];
            if (value.EndsWith('|') && !value.EndsWith("\\|", StringComparison.Ordinal))
                value = value[..^1];
        }
        List<string> cells = [];
        StringBuilder cell = new();
        bool escaped = false;
        bool inCode = false;
        foreach (char character in value)
        {
            if (escaped)
            {
                if (profile == Profile.Assistant && character != '|')
                    cell.Append('\\');
                cell.Append(character);
                escaped = false;
                continue;
            }
            if (character == '\\')
            {
                escaped = true;
                continue;
            }
            if (character == '`')
            {
                inCode = !inCode;
                cell.Append(character);
                continue;
            }
            if (character == '|' && !inCode)
            {
                cells.Add(cell.ToString().Trim());
                cell.Clear();
                continue;
            }
            cell.Append(character);
        }
        if (escaped)
            cell.Append('\\');
        cells.Add(cell.ToString().Trim());
        if (profile == Profile.Assistant)
        {
            if (cells.Count > 0 && cells[0].Length == 0)
                cells.RemoveAt(0);
            if (cells.Count > 0 && cells[^1].Length == 0)
                cells.RemoveAt(cells.Count - 1);
        }
        return cells;
    }
}
