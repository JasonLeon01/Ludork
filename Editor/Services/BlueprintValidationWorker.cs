using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class BlueprintValidationWorker : IDisposable
{
    private readonly IDisposable read;
    private BlueprintNodeDefinitionSet? generalDefinitions;

    internal BlueprintValidationWorker(string projectPath, IReadOnlyDictionary<string, JsonObject> blueprints,
        ProjectEnumCatalog enums, LuaValidationReadCache reads, bool strictReads = false,
        IReadOnlyDictionary<string, JsonObject>? generalData = null)
    {
        Metadata = reads.CreateMetadata(strictReads);
        Resolver = new BlueprintClassResolver(Metadata, key => blueprints.GetValueOrDefault(key));
        read = Resolver.BeginBatch();
        Validation = new BlueprintValidationService(projectPath, blueprints, generalData ??
            new Dictionary<string, JsonObject>(StringComparer.Ordinal), enums, Metadata, Resolver);
    }

    internal LuaMetadataService Metadata { get; }
    internal BlueprintClassResolver Resolver { get; }
    internal BlueprintValidationService Validation { get; }
    internal BlueprintNodeDefinitionSet GeneralDefinitions => generalDefinitions ??=
        new BlueprintNodeDefinitionCatalog(Metadata, Resolver).GetNodeDefinitionSet();

    public void Dispose()
    {
        read.Dispose();
        Resolver.Dispose();
    }
}
