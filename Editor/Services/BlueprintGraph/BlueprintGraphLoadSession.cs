using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class BlueprintGraphLoadSession : IDisposable
{
    private CancellationTokenSource? active;

    internal CancellationToken Begin()
    {
        Cancel();
        active = new CancellationTokenSource();
        return active.Token;
    }

    internal bool IsCurrent(CancellationToken token) => active is not null
        && active.Token == token && !token.IsCancellationRequested;

    internal void Complete(CancellationToken token)
    {
        if (!IsCurrent(token))
            return;
        active!.Dispose();
        active = null;
    }

    internal void Cancel()
    {
        CancellationTokenSource? cancellation = active;
        active = null;
        cancellation?.Cancel();
        cancellation?.Dispose();
    }

    public void Dispose() => Cancel();

    internal async Task<BlueprintGraphDocument> LoadAsync(
        string eventName,
        JsonObject eventGraph,
        JsonNode? startNode,
        BlueprintNodeDefinitionSet definitionSet,
        IReadOnlyList<BlueprintGraphEventParameterDefinition> eventParameters,
        bool inheritsEvents,
        Func<IDisposable> beginMetadataRead,
        Func<bool> isCurrent,
        CancellationToken cancellationToken)
    {
        string language = LocaleService.CurrentLanguage;
        string entryTitle = LocaleService.Get("BLUEPRINT_EVENT_ENTRY");
        EditorUiBatch batch = new();
        JsonNode? start = startNode?.DeepClone();
        JsonObject graph = new();
        foreach (KeyValuePair<string, JsonNode?> field in eventGraph.ToArray())
        {
            checkCurrent();
            if (field.Key is "nodes" or "links" && field.Value is JsonArray items)
            {
                JsonArray copied = new();
                graph[field.Key] = copied;
                foreach (JsonNode? item in items.ToArray())
                {
                    checkCurrent();
                    copied.Add(item?.DeepClone());
                    await batch.YieldIfNeededAsync(cancellationToken);
                }
            }
            else
                graph[field.Key] = field.Value?.DeepClone();
            await batch.YieldIfNeededAsync(cancellationToken);
        }
        Dictionary<string, BlueprintGraphNodeDefinition> lookup = new(StringComparer.Ordinal);
        Dictionary<BlueprintGraphNodeDefinition, BlueprintGraphNodeDefinition> definitions = [];
        Dictionary<string, LuaEnumDefinition> enums = new(StringComparer.Ordinal);
        foreach (JsonObject node in (graph["nodes"] as JsonArray ?? []).OfType<JsonObject>())
        {
            await batch.YieldIfNeededAsync(cancellationToken);
            checkCurrent();
            if (node["nodeFunction"] is not JsonValue value || !value.TryGetValue(out string? path)
                || path is null || lookup.ContainsKey(path)
                || !definitionSet.RuntimeLookup.TryGetValue(path, out BlueprintGraphNodeDefinition? source))
                continue;
            if (!definitions.TryGetValue(source, out BlueprintGraphNodeDefinition? prepared))
            {
                using (IDisposable read = beginMetadataRead())
                    prepared = captureDefinition(source, enums);
                definitions[source] = prepared;
            }
            lookup[path] = prepared;
            await batch.YieldIfNeededAsync(cancellationToken);
        }
        checkCurrent();
        BlueprintNodeDefinitionSet frozen = new(definitions.Values.ToArray(), lookup,
            new Dictionary<string, IReadOnlyList<BlueprintGraphEventParameterDefinition>>(StringComparer.Ordinal));
        BlueprintGraphEventParameterDefinition[] parameters = eventParameters.ToArray();
        BlueprintGraphDocument result = await Task.Run(() => BlueprintGraphCodec.Load(eventName,
            graph, start, frozen, parameters, inheritsEvents, cancellationToken, entryTitle), cancellationToken);
        checkCurrent();
        return result;

        void checkCurrent()
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (IsCurrent(cancellationToken) && isCurrent()
                && string.Equals(language, LocaleService.CurrentLanguage, StringComparison.Ordinal))
                return;
            if (IsCurrent(cancellationToken))
                Cancel();
            throw new OperationCanceledException(cancellationToken);
        }
    }

    private static BlueprintGraphNodeDefinition captureDefinition(BlueprintGraphNodeDefinition source,
        Dictionary<string, LuaEnumDefinition> enums)
    {
        List<BlueprintGraphPortDefinition> ports = new(source.Ports.Count);
        foreach (BlueprintGraphPortDefinition port in source.Ports)
        {
            captureEnums(LuaMetadataType.Parse(port.TypeName), port.ResolveEnum, enums);
            ports.Add(new BlueprintGraphPortDefinition(port.Name, port.Kind, port.Direction,
                port.PinIndex, port.TypeName, port.ParameterIndex, port.SupportsEditor,
                port.DefaultValue, port.Meta, resolveEnum));
        }
        return new BlueprintGraphNodeDefinition(source.RuntimePath, ports, source.Meta,
            source.RuntimeAliases.ToArray(), source.MetadataPath, source.MemberName, source.DeclaringType,
            source.IsParent, source.IsContextRelevant, source.IsLatent);

        LuaEnumDefinition resolveEnum(LuaMetadataType type) => enums.GetValueOrDefault(type.ToString())
            ?? new LuaEnumDefinition(type.Name, null, [], "Enum type was not captured: " + type);
    }

    private static void captureEnums(LuaMetadataType type,
        Func<LuaMetadataType, LuaEnumDefinition>? resolve,
        Dictionary<string, LuaEnumDefinition> enums)
    {
        if (type.Kind == LuaMetadataTypeKind.Enum && !enums.ContainsKey(type.ToString()))
        {
            LuaEnumDefinition value = resolve?.Invoke(type)
                ?? new LuaEnumDefinition(type.Name, null, [], "Enum resolver is unavailable for " + type.Name);
            enums[type.ToString()] = value with { Options = value.Options.Select(option => option.Clone()).ToArray() };
        }
        foreach (LuaMetadataType argument in type.Arguments)
            captureEnums(argument, resolve, enums);
    }
}
