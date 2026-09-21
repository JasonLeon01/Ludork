using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GeneralDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection generalDocuments;

    internal GeneralDataService(ProjectDataStore store, EditorDocumentCollection generalDocuments)
    {
        this.store = store;
        this.generalDocuments = generalDocuments;

    }

    public IReadOnlyDictionary<string, GeneralDataTypeSnapshot> GeneralData => new DocumentSnapshotDictionary<GeneralDataTypeSnapshot>(generalDocuments, value => new GeneralDataTypeSnapshot(value));

}
