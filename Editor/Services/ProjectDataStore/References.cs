using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ProjectDataStore
{
    private LuaMetadataService? metadata;
    private BlueprintClassResolver? blueprintClasses;
    private ReferenceIndexService? referenceIndex;

    public LuaMetadataService Metadata => metadata ??= new LuaMetadataService(ProjectPath, enums: Enums);
    public BlueprintClassResolver BlueprintClasses => blueprintClasses ??= new BlueprintClassResolver(this, Metadata);
    public ReferenceIndexService ReferenceIndex => referenceIndex ??= new ReferenceIndexService(this, Metadata, BlueprintClasses);

    internal JsonObject? ReadReferenceBlueprint(string key) => sections["Blueprints"].TryGetValue(key, out JsonObject? data)
        ? (JsonObject)data.DeepClone() : null;

    internal IReadOnlyDictionary<string, JsonObject> GetReferenceSection(string section) => sections[section];

    internal IEnumerable<(string Section, string Key, JsonObject Data)> ReferenceDocuments => sections
        .Where(pair => pair.Value.Persist)
        .SelectMany(section => section.Value.Select(pair => (section.Key, pair.Key, pair.Value)));
}
