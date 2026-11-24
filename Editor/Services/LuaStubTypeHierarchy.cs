using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;

namespace Ludork.Services;

internal sealed class LuaStubTypeHierarchy
{
    private static readonly Regex ClassAnnotation = new(
        @"^---\s*@class(?:\s+\([^)]*\))?\s+([A-Za-z_][\w.]*)(.*)$",
        RegexOptions.CultureInvariant | RegexOptions.Compiled);
    private static readonly Regex TypeName = new(
        @"^[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*$",
        RegexOptions.CultureInvariant | RegexOptions.Compiled);
    private readonly string directory;
    private readonly Dictionary<string, string[]> bases = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (DateTime Modified, long Length)> stamps = new(StringComparer.OrdinalIgnoreCase);

    public LuaStubTypeHierarchy(string directory, CancellationToken cancellationToken = default)
    {
        this.directory = directory;
        string[] paths = getPaths();
        ((DateTime Modified, long Length) Stamp, Dictionary<string, List<string>> Types)[] files =
            EditorValidationBatch.RunFiles(paths, path => readDeclarations(path, cancellationToken), cancellationToken);
        for (int index = 0; index < files.Length; index++)
        {
            stamps[paths[index]] = files[index].Stamp;
            foreach ((string name, List<string> parents) in files[index].Types)
                bases[name] = bases.TryGetValue(name, out string[]? existing)
                    ? existing.Concat(parents).Distinct(StringComparer.Ordinal).ToArray() : parents.ToArray();
        }
    }

    private static ((DateTime Modified, long Length) Stamp, Dictionary<string, List<string>> Types) readDeclarations(
        string path, CancellationToken cancellationToken)
    {
        FileInfo info = new(path);
        (DateTime Modified, long Length) stamp = (info.LastWriteTimeUtc, info.Length);
        Dictionary<string, List<string>> declarations = new(StringComparer.Ordinal);
        foreach (string line in File.ReadLines(path))
        {
            cancellationToken.ThrowIfCancellationRequested();
            Match annotation = ClassAnnotation.Match(line.TrimStart());
            if (!annotation.Success)
                continue;
            string name = annotation.Groups[1].Value;
            declarations.TryAdd(name, []);
            string remainder = annotation.Groups[2].Value.Trim();
            if (remainder.StartsWith('<'))
            {
                int end = closingGeneric(remainder);
                if (end < 0)
                    continue;
                remainder = remainder[(end + 1)..].TrimStart();
            }
            if (!remainder.StartsWith(':'))
                continue;
            List<string> parents = declarations[name];
            foreach (string declaration in splitBases(remainder[1..]))
            {
                string parent = declaration.Trim();
                int generic = parent.IndexOf('<');
                if (generic >= 0)
                    parent = parent[..generic].TrimEnd();
                if (TypeName.IsMatch(parent) && !parents.Contains(parent, StringComparer.Ordinal))
                    parents.Add(parent);
            }
        }
        return (stamp, declarations);
    }

    public IEnumerable<string> TypeNames => bases.Keys;

    public IReadOnlyList<string> GetBases(string typeName)
    {
        return bases.GetValueOrDefault(typeName) ?? [];
    }

    public bool IsCurrent()
    {
        string[] paths = getPaths();
        if (paths.Length != stamps.Count)
            return false;
        return EditorValidationBatch.RunFiles(paths, path =>
        {
            FileInfo info = new(path);
            return stamps.TryGetValue(path, out (DateTime Modified, long Length) stamp)
                && stamp == (info.LastWriteTimeUtc, info.Length);
        }).All(current => current);
    }

    private string[] getPaths()
    {
        return Directory.Exists(directory)
            ? Directory.GetFiles(directory, "*.d.lua", SearchOption.AllDirectories)
            : [];
    }

    private static int closingGeneric(string text)
    {
        int depth = 0;
        for (int index = 0; index < text.Length; index++)
        {
            if (text[index] == '<')
                depth++;
            else if (text[index] == '>' && --depth == 0)
                return index;
        }
        return -1;
    }

    private static IEnumerable<string> splitBases(string text)
    {
        int depth = 0;
        int start = 0;
        for (int index = 0; index < text.Length; index++)
        {
            if (text[index] is '<' or '(' or '[' or '{')
                depth++;
            else if (text[index] is '>' or ')' or ']' or '}')
                depth--;
            else if (text[index] == ',' && depth == 0)
            {
                yield return text[start..index];
                start = index + 1;
            }
        }
        yield return text[start..];
    }
}
