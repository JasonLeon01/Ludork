using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class BlueprintDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection blueprintDocuments;
    private readonly EditorDocumentCollection commonFunctionDocuments;

    internal BlueprintDataService(ProjectDataStore store, EditorDocumentCollection blueprintDocuments, EditorDocumentCollection commonFunctionDocuments)
    {
        this.store = store;
        this.blueprintDocuments = blueprintDocuments;
        this.commonFunctionDocuments = commonFunctionDocuments;

    }

    public IReadOnlyDictionary<string, CommonFunctionSnapshot> CommonFunctionsData => new DocumentSnapshotDictionary<CommonFunctionSnapshot>(commonFunctionDocuments, value => new CommonFunctionSnapshot(value));

    public IReadOnlyDictionary<string, BlueprintDefinitionSnapshot> BlueprintsData => new DocumentSnapshotDictionary<BlueprintDefinitionSnapshot>(blueprintDocuments, value => new BlueprintDefinitionSnapshot(value));

}
