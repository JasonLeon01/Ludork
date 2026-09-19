using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private void scanResolvedFieldReferences(
        string sourceId,
        ResolvedBlueprintClass resolved,
        string path,
        string kind,
        JsonObject? selectedFields = null)
    {
        foreach (ResolvedBlueprintField field in resolved.Fields)
        {
            if (selectedFields is not null && !selectedFields.ContainsKey(field.Name))
                continue;
            string fieldPath = $"{path}.{field.Name}";
            scanGenericReferences(sourceId, field.Value, fieldPath);
            scanTypedReferences(sourceId, field.Type.Schema, field.Value, field.Name,
                field.Metadata?.Meta, resolved.Meta, field.Metadata?.DeclaringType.ModuleName,
                fieldPath, kind, []);
        }
    }

    private void scanTypedReferences(
        string sourceId,
        LuaMetadataType type,
        JsonNode? value,
        string name,
        JsonObject? meta,
        JsonObject? ownerMeta,
        string? defaultModule,
        string path,
        string kind,
        HashSet<(string Type, JsonNode? Value)> resolving)
    {
        cancellationToken.ThrowIfCancellationRequested();
        scanAnnotatedReference(sourceId, value, name, meta, ownerMeta, path, kind);
        if (type.Kind == LuaMetadataTypeKind.Union)
        {
            if (value is JsonObject wrapper && wrapper.ContainsKey("$type") && wrapper.ContainsKey("$value"))
            {
                LuaMetadataType? branch = type.Arguments.FirstOrDefault(candidate =>
                    JsonNode.DeepEquals(candidate.ToSchema(), wrapper["$type"]));
                if (branch is not null)
                    scanTypedReferences(sourceId, branch, wrapper["$value"], name, meta, ownerMeta,
                        defaultModule, path + ".$value", kind, resolving);
            }
            return;
        }
        if (type.Kind is LuaMetadataTypeKind.List or LuaMetadataTypeKind.Tuple)
        {
            if (value is not JsonArray items)
                return;
            for (int index = 0; index < items.Count; index++)
            {
                if (type.Kind == LuaMetadataTypeKind.Tuple && index >= type.Arguments.Count)
                    break;
                LuaMetadataType itemType = type.Arguments[type.Kind == LuaMetadataTypeKind.List ? 0 : index];
                scanTypedReferences(sourceId, itemType, items[index], name, meta, ownerMeta,
                    defaultModule, $"{path}[{index}]", kind, resolving);
            }
            return;
        }
        if (type.Kind == LuaMetadataTypeKind.Dictionary)
        {
            if (value is JsonObject dictionary)
            {
                foreach (KeyValuePair<string, JsonNode?> item in dictionary)
                    scanTypedReferences(sourceId, type.Arguments[1], item.Value, name, meta, ownerMeta,
                        defaultModule, $"{path}.{item.Key}", kind, resolving);
            }
            return;
        }
        if (type.Kind != LuaMetadataTypeKind.Named || value is not null and not JsonObject)
            return;
        LuaTypeReference reference = LuaTypeReference.FromSchema(type).WithDefaultModule(defaultModule);
        if (metadataService.GetType(reference) is null || !resolving.Add((reference.QualifiedName, value)))
            return;
        try
        {
            JsonObject? supplied = value as JsonObject;
            ResolvedBlueprintClass structure = classResolver.Resolve(reference.QualifiedName, supplied);
            foreach (ResolvedBlueprintField field in structure.Fields)
            {
                cancellationToken.ThrowIfCancellationRequested();
                JsonNode? child = null;
                bool hasValue = supplied?.TryGetPropertyValue(field.Name, out child) == true;
                if (!hasValue)
                {
                    if (field.Metadata?.HasDefaultValue == true)
                        child = field.Metadata.DefaultValue;
                    else if (field.Metadata?.Component != true)
                        continue;
                }
                string fieldPath = $"{path}.{field.Name}";
                scanGenericReferences(sourceId, child, fieldPath);
                scanTypedReferences(sourceId, field.Type.Schema, child, field.Name,
                    field.Metadata?.Meta, structure.Meta, field.Metadata?.DeclaringType.ModuleName,
                    fieldPath, kind, resolving);
            }
        }
        finally
        {
            resolving.Remove((reference.QualifiedName, value));
        }
    }

    private void scanAnnotatedReference(
        string sourceId,
        JsonNode? value,
        string name,
        JsonObject? meta,
        JsonObject? ownerMeta,
        string path,
        string kind)
    {
        string? getReference(string key) =>
            getMetaReference(meta?[key], name) ?? getMetaReference(ownerMeta?[key], name);

        if (getReference("PathVars") is not null && getReference("PathRoot") != "Project")
            addAssetReference(sourceId, value, kind, path);
        if (getReference("BlueprintClassVars") is not null
            && blueprintNodeIdFromClassPath(value) is string blueprintId)
            addReference(sourceId, blueprintId, kind, path);
        if (getReference("CommonFunctionVars") is not null
            && normalizeReferenceParam(value) is string functionName)
            addReference(sourceId, nodeId("commonFunction", functionName), kind, path);
        if (getReference("GeneralDataVars") is string generalType
            && normalizeReferenceParam(value) is string generalValue)
        {
            string target = generalType.ToUpperInvariant() switch
            {
                "ANIMATION" => nodeId("animation", generalValue),
                "PARTICLE" => nodeId("particle", generalValue),
                _ => generalMemberNodeId(generalType, generalValue),
            };
            addReference(sourceId, target, kind, path);
        }
    }
}
