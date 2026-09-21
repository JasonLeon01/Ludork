using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class UiAssetDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection uiDocuments;

    internal UiAssetDataService(ProjectDataStore store, EditorDocumentCollection uiDocuments)
    {
        this.store = store;
        this.uiDocuments = uiDocuments;

    }

    public event EventHandler? UiAssetsChanged;

    public IReadOnlyDictionary<string, UiAssetSnapshot> UiAssetsData => new DocumentSnapshotDictionary<UiAssetSnapshot>(uiDocuments
        .Where(pair => ProjectDataStore.isUiDataType(pair.Value, UiAssetSchema.UiAssetType))
        .ToDictionary(pair => pair.Key, pair => pair.Value, StringComparer.Ordinal), value => new UiAssetSnapshot(value));

}
