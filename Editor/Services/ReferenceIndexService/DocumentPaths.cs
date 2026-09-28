using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private static readonly StringComparer DocumentPathComparer = OperatingSystem.IsWindows()
        ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal;
    private Dictionary<string, List<string>>? nodesByDocumentPath;
    private string[] indexedDocumentPaths = [];

    private IReadOnlyList<string> findDocumentNodes(string fullPath, bool includeDescendants)
    {
        if (nodesByDocumentPath is null)
        {
            nodesByDocumentPath = new Dictionary<string, List<string>>(DocumentPathComparer);
            foreach (string id in nodes.Keys)
            {
                string path = ReferenceIndexSnapshot.ResolvePath(gameData.ProjectPath, id, generalMemberTypes);
                if (path.Length == 0)
                    continue;
                if (!nodesByDocumentPath.TryGetValue(path, out List<string>? ids))
                {
                    ids = [];
                    nodesByDocumentPath.Add(path, ids);
                }
                ids.Add(id);
            }
            indexedDocumentPaths = nodesByDocumentPath.Keys.OrderBy(path => path, DocumentPathComparer).ToArray();
        }
        List<string> result = [];
        if (nodesByDocumentPath.TryGetValue(fullPath, out List<string>? exact))
            result.AddRange(exact);
        if (!includeDescendants)
            return result;
        string prefix = fullPath + Path.DirectorySeparatorChar;
        int start = Array.BinarySearch(indexedDocumentPaths, prefix, DocumentPathComparer);
        if (start < 0)
            start = ~start;
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        for (int index = start; index < indexedDocumentPaths.Length; index++)
        {
            string path = indexedDocumentPaths[index];
            if (!path.StartsWith(prefix, comparison))
                break;
            result.AddRange(nodesByDocumentPath[path]);
        }
        return result;
    }
}
