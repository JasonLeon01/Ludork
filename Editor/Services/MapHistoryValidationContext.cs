using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class MapHistoryValidationContext(string projectPath,
    IReadOnlyDictionary<string, JsonObject> blueprints, ProjectEnumCatalog enums)
{
    internal MapHistoryValidationResult Validate(string mapKey, IReadOnlyList<MapHistoryActorChange> changes)
    {
        LuaReadDependencyCapture dependencies = new();
        LuaEnumService enumService = new(projectPath, () => enums, null, dependencies);
        LuaMetadataService metadata = new(projectPath, true, default, enumService, null, dependencies);
        DocumentReferenceResult references;
        using (IDisposable metadataRead = metadata.BeginReferenceRead())
        using (IDisposable enumRead = enumService.BeginRead())
        using (BlueprintClassResolver resolver = new(metadata, readBlueprint))
        {
            DocumentReferenceScanner scanner = new(metadata, resolver, () => enums);
            references = scanner.ScanMapHistoryReferences(mapKey, changes);
        }
        return new MapHistoryValidationResult(references, dependencies.Capture());

        JsonObject? readBlueprint(string key)
        {
            string path = Path.Combine(projectPath, "Data", "Blueprints",
                key.Replace('/', Path.DirectorySeparatorChar) + ".json");
            dependencies.Record(path, LuaMetadataFileStamp.Read(path));
            return blueprints.GetValueOrDefault(key);
        }
    }
}
