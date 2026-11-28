using Ludork.Models;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Frozen;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

internal sealed class PluginBlueprintSnapshot : IPluginBlueprintSnapshot
{
    private readonly PluginBlueprintWorkspace workspace;
    private readonly IReadOnlyList<PluginBlueprintSource> managedSources;
    private readonly ProjectEnumCatalog enums;
    private readonly LuaValidationReadCache reads;
    private readonly string dataRoot;
    private readonly string blueprintRoot;

    internal PluginBlueprintSnapshot(PluginBlueprintWorkspace workspace, IReadOnlyList<PluginBlueprintSource> managedSources,
        FrozenSet<string> paths, StringComparer comparer, ProjectEnumCatalog enums, long revision)
    {
        this.workspace = workspace;
        this.managedSources = managedSources;
        this.enums = enums;
        reads = new LuaValidationReadCache(workspace.ProjectPath, enums);
        dataRoot = Path.Combine(workspace.ProjectPath, "Data");
        blueprintRoot = Path.Combine(dataRoot, "Blueprints");
        Blueprints = managedSources.ToDictionary(source => GetKey(source.Path), source => source.Data, StringComparer.Ordinal);
        Revision = revision;
        PathComparer = comparer;
        ManagedBlueprintPaths = paths;
        GeneralData = enums.GeneralDataModules.ToFrozenDictionary(pair => pair.Key,
            pair => new PluginGeneralDataType(pair.Value, Array.AsReadOnly(enums.Definitions[pair.Value].Options
                .Select(option => option.Value!.GetValue<string>()).ToArray())), StringComparer.Ordinal);
    }

    internal long Revision { get; }
    internal IReadOnlyDictionary<string, JsonObject> Blueprints { get; }
    public StringComparer PathComparer { get; }
    public IReadOnlySet<string> ManagedBlueprintPaths { get; }
    public IReadOnlyDictionary<string, PluginGeneralDataType> GeneralData { get; }

    public PluginBlueprintClass? DescribeClass(string reference)
    {
        if (string.IsNullOrWhiteSpace(reference) || !reference.Split('.').All(LuaIdentifier.IsValid))
            return null;
        using BlueprintValidationWorker worker = CreateWorker();
        LuaTypeMetadata? metadata = worker.Metadata.GetRuntimeClassType(reference);
        if (metadata is null)
            return null;
        ResolvedBlueprintClass resolved = worker.Resolver.Resolve(reference);
        HashSet<string> hidden = new(resolved.InvalidVars, StringComparer.Ordinal);
        Dictionary<string, JsonNode> fields = resolved.Fields
            .Where(field => !field.Name.StartsWith('_') && !hidden.Contains(field.Name))
            .ToDictionary(field => field.Name, field =>
                field.Type.WithDefaultModule(field.Metadata?.DeclaringType.ModuleName).Schema.ToSchema(), StringComparer.Ordinal);
        return new PluginBlueprintClass(worker.Metadata.GetRuntimeClassReference(metadata.Type),
            getAncestors(worker.Metadata, metadata.Type), fields);
    }

    public IReadOnlyList<PluginResolvedBlueprint> ResolveBlueprints(IReadOnlyList<PluginBlueprintSource> additional,
        IReadOnlyList<string> fields, CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string[] requested = fields.Distinct(StringComparer.Ordinal).ToArray();
        (string Path, JsonObject? Data)[] sources = EditorValidationBatch.RunFiles(additional, source =>
        {
            if (!Path.IsPathFullyQualified(source.Path) || !EditorPathSandbox.IsSameOrChildPath(dataRoot, source.Path)
                || !source.Path.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                throw new ArgumentException($"Expected a JSON path under Data: {source.Path}");
            string path = Path.GetFullPath(source.Path);
            if (ManagedBlueprintPaths.Contains(path))
                return (path, (JsonObject?)null);
            if (source.Data.TryGetPropertyValue("type", out JsonNode? type) && !JsonNode.DeepEquals(type, "blueprint"))
                throw new InvalidDataException($"Expected Blueprint data: {path}");
            return (path, (JsonObject?)source.Data.DeepClone());
        }, cancellationToken);
        Dictionary<string, JsonObject> parents = new(Blueprints, StringComparer.Ordinal);
        List<PluginBlueprintSource> documents = [.. managedSources];
        HashSet<string> paths = new(PathComparer);
        foreach ((string path, JsonObject? data) in sources)
        {
            if (data is null)
                continue;
            if (!paths.Add(path))
                throw new ArgumentException($"More than one Blueprint source has the path '{path}'.");
            documents.Add(new(path, data));
            if (EditorPathSandbox.IsSameOrChildPath(blueprintRoot, path))
                parents.Add(GetKey(path), data);
        }
        return EditorValidationBatch.RunFiles(documents, () => CreateWorker(parents),
            (worker, source) =>
            {
                ResolvedBlueprintClass resolved = worker.Resolver.ResolveBlueprint(source.Data, GetKey(source.Path));
                Dictionary<string, JsonNode?> values = new(StringComparer.Ordinal);
                foreach (string name in requested)
                    if (resolved.GetField(name) is ResolvedBlueprintField field)
                        values.Add(name, field.Value?.DeepClone());
                return new PluginResolvedBlueprint(source.Path, getAncestors(worker.Metadata, resolved.RootType!), values);
            }, worker => worker.Dispose(), cancellationToken);
    }

    public PluginBlueprintDestination CheckDestination(string projectRelativePath)
        => ResolveDestination(projectRelativePath, out _);

    internal PluginBlueprintDestination ResolveDestination(string relativePath, out string path)
    {
        path = string.Empty;
        if (string.IsNullOrWhiteSpace(relativePath) || !relativePath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
            return PluginBlueprintDestination.Invalid;
        string normalized = relativePath.Replace('\\', '/');
        string[] parts = normalized[..^".json".Length].Split('/');
        if (parts.Length < 3 || parts[0] != "Data" || parts[1] != "Blueprints" || parts.Skip(2).Any(part =>
                !ProjectNameValidation.IsValidDirectoryName(part) || part.IndexOfAny(['.', ':', '*', '?', '"', '<', '>', '|']) >= 0
                || part.EndsWith(' ') || part.Any(char.IsControl))
            || normalized.Split('/').Any(part => Encoding.UTF8.GetByteCount(part) > 255)
            || !EditorPathSandbox.TryResolve(workspace.ProjectPath, normalized, out path, allowMissing: true))
            return PluginBlueprintDestination.Invalid;
        return ManagedBlueprintPaths.Contains(path) || File.Exists(path) || Directory.Exists(path)
            ? PluginBlueprintDestination.Occupied : PluginBlueprintDestination.Available;
    }

    public Task CreateAsync(IReadOnlyList<PluginBlueprintCreation> blueprints, CancellationToken cancellationToken)
    {
        PluginBlueprintCreation[] inputs = EditorValidationBatch.RunFiles(blueprints,
            creation => creation with { Attributes = (JsonObject)creation.Attributes.DeepClone() }, cancellationToken);
        return workspace.CreateAsync(this, inputs, cancellationToken);
    }

    internal string GetKey(string absolutePath)
        => Path.ChangeExtension(Path.GetRelativePath(blueprintRoot, absolutePath), null)!.Replace('\\', '/');

    internal BlueprintValidationWorker CreateWorker(IReadOnlyDictionary<string, JsonObject>? parents = null)
        => new(workspace.ProjectPath, parents ?? Blueprints, enums, reads, true);

    private static string[] getAncestors(LuaMetadataService metadata, LuaTypeReference type)
        => metadata.ResolveMro(type).Select(ancestor => metadata.GetRuntimeClassReference(ancestor.Type)).ToArray();
}
