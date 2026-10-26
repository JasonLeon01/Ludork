using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    internal DocumentReferenceResult ScanMapHistoryReferences(string mapKey, IReadOnlyList<MapHistoryActorChange> changes)
    {
        projectEnums = getProjectEnums();
        references.Clear();
        seen.Clear();
        generalMemberTypes.Clear();
        string source = ReferenceIdentity.NodeId("map", mapKey);
        Dictionary<(string Path, bool IsDictionaryKey), string> storedValues = [];
        foreach (MapHistoryActorChange change in changes)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (change.Fields is { Count: 0 })
                continue;
            string actorPath = $"Maps/{mapKey}.actors.{change.Layer}[{change.Index}]";
            string? classReference = JsonScalar.String(change.Actor["bp"]);
            if (string.IsNullOrWhiteSpace(classReference))
                throw new InvalidDataException(actorPath + ".bp must be a class reference");
            string? tag = JsonScalar.String(change.Actor["tag"]);
            string overridesPath = $"Maps/{mapKey}.BPClassVarChanged.{tag}";
            JsonObject? overrides = change.Overrides;
            if (change.Fields is not null && overrides is not null)
                overrides = new JsonObject(overrides.Where(pair => change.Fields.Contains(pair.Key))
                    .Select(pair => new KeyValuePair<string, JsonNode?>(pair.Key, pair.Value?.DeepClone())));
            ResolvedBlueprintClass resolved = change.Fields is null
                ? classResolver.Resolve(classReference, overrides)
                : classResolver.ResolveFields(classReference, overrides, change.Fields);
            scanResolvedFieldReferences(source, resolved, overridesPath, "mapActor", overrides, change.Fields);
            if (overrides is not null)
            {
                scanGenericReferences(source, overrides, overridesPath);
                scanKnownMapNodeReferences(source, overrides, overridesPath);
                collectStoredValues(overrides, overridesPath, "/BPClassVarChanged/" + escapePointer(tag ?? string.Empty), storedValues);
            }
            if (change.Fields is null)
            {
                if (blueprintNodeIdFromClassPath(change.Actor["bp"]) is string blueprint)
                    addReference(source, blueprint, "mapActor", actorPath + ".bp");
                scanGenericReferences(source, change.Actor, actorPath);
                scanKnownMapNodeReferences(source, change.Actor, actorPath);
                collectStoredValues(change.Actor, actorPath,
                    "/actors/" + escapePointer(change.Layer) + "/" + change.Index, storedValues);
            }
        }
        return new DocumentReferenceResult(
            Array.AsReadOnly(references.Select(reference => reference with
            {
                StoragePath = reference.StoragePath is null ? null
                    : storedValues.GetValueOrDefault((reference.Path, reference.IsDictionaryKey)),
            }).GroupBy(reference => (reference.Source, reference.Target, reference.Kind, reference.Path, reference.IsDictionaryKey))
                .Select(group => group.FirstOrDefault(reference => reference.StoragePath is not null) ?? group.First()).ToArray()),
            new ReadOnlyDictionary<string, string>(new Dictionary<string, string>(generalMemberTypes, StringComparer.Ordinal)));
    }
}
