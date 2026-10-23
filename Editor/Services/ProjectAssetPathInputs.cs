using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace Ludork.Services;

internal sealed class ProjectAssetPathInputs
{
    private readonly IReadOnlyList<EditorDocumentState> entries;

    internal ProjectAssetPathInputs(IReadOnlyList<EditorDocumentState> entries) => this.entries = entries;

    internal void Validate()
    {
        HashSet<string> tracked = entries.Select(entry => entry.Path).ToHashSet(StringComparer.Ordinal);
        foreach (EditorDocumentState entry in entries)
        {
            if (entry.InternalData?["directory"]?.GetValue<bool>() == true)
            {
                if (!Directory.Exists(entry.Path))
                    throw new IOException($"The asset directory changed outside the editor: {entry.Path}");
                foreach (string path in Directory.EnumerateFileSystemEntries(entry.Path, "*", SearchOption.TopDirectoryOnly))
                    if (!tracked.Contains(path))
                        throw new IOException($"The asset directory changed outside the editor: {path}");
            }
            else
            {
                FileInfo file = new(entry.Path);
                if (!file.Exists || file.Length != entry.InternalData?["length"]?.GetValue<long>()
                    || file.LastWriteTimeUtc.Ticks != entry.InternalData?["modified"]?.GetValue<long>())
                    throw new IOException($"The asset changed outside the editor: {entry.Path}");
            }
        }
    }
}
