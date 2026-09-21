using Ludork.Models;
using System;
using System.Diagnostics.CodeAnalysis;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class WorldDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection worldDocuments;

    internal WorldDataService(ProjectDataStore store, EditorDocumentCollection worldDocuments)
    {
        this.store = store;
        this.worldDocuments = worldDocuments;

        MapPathPolicy = new WorldMapPathPolicy(store.ProjectPath);
    }

    internal readonly WorldMapValidationService worldMapValidation = new();

    private readonly Dictionary<string, string> pendingWorldDirectoryMoves = new(StringComparer.Ordinal);

    public IReadOnlyDictionary<string, WorldMapSnapshot> WorldMapData => new DocumentSnapshotDictionary<WorldMapSnapshot>(worldDocuments, value => new WorldMapSnapshot(value));

    public WorldMapPathPolicy MapPathPolicy { get; }

    internal void ClearPendingDirectoryMoves() => pendingWorldDirectoryMoves.Clear();

    internal void UpdatePendingDirectoryMove(string oldKey, string newKey, string? sourceDirectory)
    {
        pendingWorldDirectoryMoves.Remove(oldKey);
        if (sourceDirectory is not null)
            pendingWorldDirectoryMoves[newKey] = sourceDirectory;
    }

    internal bool TryGetPendingDirectoryMove(string key, [NotNullWhen(true)] out string? sourceDirectory) =>
        pendingWorldDirectoryMoves.TryGetValue(key, out sourceDirectory);

    internal Action CaptureDirectoryMove(string key) => ProjectDataStore.captureExternalEntry(pendingWorldDirectoryMoves, key);

}
