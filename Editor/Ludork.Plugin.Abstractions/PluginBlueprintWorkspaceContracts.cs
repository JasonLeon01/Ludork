using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugin.Abstractions;

public interface IPluginBlueprintWorkspace
{
    string ProjectPath { get; }
    Task<IPluginBlueprintSnapshot> CaptureAsync(CancellationToken cancellationToken);
}

public interface IPluginBlueprintSnapshot
{
    IReadOnlyDictionary<string, PluginGeneralDataType> GeneralData { get; }
    IReadOnlySet<string> ManagedBlueprintPaths { get; }
    StringComparer PathComparer { get; }
    PluginBlueprintClass? DescribeClass(string reference);
    IReadOnlyList<PluginResolvedBlueprint> ResolveBlueprints(IReadOnlyList<PluginBlueprintSource> additional,
        IReadOnlyList<string> fields, CancellationToken cancellationToken);
    PluginBlueprintDestination CheckDestination(string projectRelativePath);
    Task CreateAsync(IReadOnlyList<PluginBlueprintCreation> blueprints, CancellationToken cancellationToken);
}

public sealed record PluginGeneralDataType(string EnumModule, IReadOnlyList<string> Members);
public sealed record PluginBlueprintClass(string Reference, IReadOnlyList<string> Ancestors,
    IReadOnlyDictionary<string, JsonNode> EditableFields);
public sealed record PluginBlueprintSource(string Path, JsonObject Data);
public sealed record PluginResolvedBlueprint(string Path, IReadOnlyList<string> Ancestors, IReadOnlyDictionary<string, JsonNode?> Values);
public sealed record PluginBlueprintCreation(string Path, string Parent, JsonObject Attributes);

public enum PluginBlueprintDestination
{
    Available,
    Occupied,
    Invalid,
}
