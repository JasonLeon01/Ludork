using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    public void AcceptTrashedResources(IReadOnlyList<string> paths)
    {
        foreach (string path in paths)
        {
            if (File.Exists(path) || Directory.Exists(path))
                throw new InvalidOperationException($"The resource has not been moved to Trash: {path}");
        }
        Dictionary<(string Section, string Key), JsonObject?> changes = [];
        foreach (string path in paths)
        {
            deletedDocumentPaths.Add(path);
            EditorDocument? document = GetDocumentByPath(path);
            if (document is not null)
                deletedDocumentPaths.Add(document.SavedPath);
            prepareExternalDelete(path, changes);
        }
        applyExternalChanges(changes, true, null);
        DataReloaded?.Invoke(this, EventArgs.Empty);
        NotifyDataRestored();
    }

}
