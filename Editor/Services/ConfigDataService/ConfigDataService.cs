using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ConfigDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection configDocuments;

    internal ConfigDataService(ProjectDataStore store, EditorDocumentCollection configDocuments)
    {
        this.store = store;
        this.configDocuments = configDocuments;

    }

    public IReadOnlyDictionary<string, ConfigSnapshot> SystemConfigData => new DocumentSnapshotDictionary<ConfigSnapshot>(configDocuments, value => new ConfigSnapshot(value));

}
