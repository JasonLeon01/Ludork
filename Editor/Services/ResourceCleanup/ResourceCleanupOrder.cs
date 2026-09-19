using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Ludork.Services;

internal static class ResourceCleanupOrder
{
    public static IReadOnlyList<string> Create(
        IReadOnlySet<string> candidates,
        IReadOnlyDictionary<string, HashSet<string>> edges,
        CancellationToken token)
    {
        Dictionary<string, string[]> outgoing = candidates.ToDictionary(path => path,
            path => edges[path].Where(candidates.Contains).OrderBy(value => value, StringComparer.Ordinal).ToArray(),
            StringComparer.Ordinal);
        Dictionary<string, List<string>> incoming = candidates.ToDictionary(path => path,
            _ => new List<string>(), StringComparer.Ordinal);
        foreach (KeyValuePair<string, string[]> pair in outgoing)
        {
            foreach (string target in pair.Value)
                incoming[target].Add(pair.Key);
        }
        HashSet<string> visited = new(StringComparer.Ordinal);
        List<string> finished = [];
        foreach (string start in candidates.OrderBy(path => path, StringComparer.Ordinal))
        {
            Stack<(string Path, bool Exit)> stack = new();
            stack.Push((start, false));
            while (stack.TryPop(out (string Path, bool Exit) item))
            {
                token.ThrowIfCancellationRequested();
                if (item.Exit)
                {
                    finished.Add(item.Path);
                    continue;
                }
                if (!visited.Add(item.Path))
                    continue;
                stack.Push((item.Path, true));
                foreach (string target in outgoing[item.Path].Reverse())
                    stack.Push((target, false));
            }
        }
        visited.Clear();
        List<string> result = [];
        foreach (string start in finished.AsEnumerable().Reverse())
        {
            if (visited.Contains(start))
                continue;
            Stack<string> stack = new();
            List<string> component = [];
            stack.Push(start);
            while (stack.TryPop(out string? current))
            {
                token.ThrowIfCancellationRequested();
                if (!visited.Add(current))
                    continue;
                component.Add(current);
                foreach (string source in incoming[current])
                    stack.Push(source);
            }
            result.AddRange(component.OrderBy(path => path, StringComparer.Ordinal));
        }
        return result;
    }
}
