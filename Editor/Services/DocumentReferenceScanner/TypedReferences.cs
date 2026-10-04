using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
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
        HashSet<(string Type, JsonNode? Value)> resolving,
        bool isDictionaryKey = false)
    {
        cancellationToken.ThrowIfCancellationRequested();
        scanEnumDependencies(sourceId, type, defaultModule, path, []);
        scanAnnotatedReference(sourceId, value, name, meta, ownerMeta, path, kind);
        if (type.Kind == LuaMetadataTypeKind.Enum)
        {
            scanEnumReference(sourceId, type, value, path, kind, isDictionaryKey);
            return;
        }
        if (type.Kind == LuaMetadataTypeKind.Named && type.Name == "file")
        {
            addAssetReference(sourceId, value, kind, path);
            return;
        }
        if (type.Kind == LuaMetadataTypeKind.Union)
        {
            if (value is JsonObject wrapper && wrapper.ContainsKey("$type") && wrapper.ContainsKey("$value"))
            {
                scanEnumSchemaReferences(sourceId, wrapper["$type"], path + ".$type");
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
                {
                    scanTypedReferences(sourceId, type.Arguments[0], JsonValue.Create(item.Key), name, null, null,
                        defaultModule, $"{path}.{item.Key}", kind, resolving, true);
                    scanTypedReferences(sourceId, type.Arguments[1], item.Value, name, meta, ownerMeta,
                        defaultModule, $"{path}.{item.Key}", kind, resolving);
                }
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

        if (getReference("PathVars") is not null)
        {
            string? root = getReference("PathRoot");
            if (root == "Data")
                addSubtitleReference(sourceId, value, kind, path);
            else if (root != "Project")
                addAssetReference(sourceId, value, kind, path);
        }
        if (getReference("BlueprintClassVars") is not null
            && blueprintNodeIdFromClassPath(value) is string blueprintId)
            addReference(sourceId, blueprintId, kind, path);
        if (getReference("CommonFunctionVars") is not null
            && ReferenceIdentity.NormalizeParameter(value) is string functionName)
            addReference(sourceId, ReferenceIdentity.NodeId("commonFunction", functionName), kind, path);
    }

    private void scanEnumReference(string sourceId, LuaMetadataType type, JsonNode? value,
        string path, string kind, bool isDictionaryKey)
    {
        if (!projectEnums.References.TryGetValue(type.Name, out ProjectEnumReference? reference)
            || JsonScalar.String(value) is not string text || text.Length == 0)
            return;
        string target = reference.Kind switch
        {
            "generalType" => ReferenceIdentity.NodeId("general", text),
            "generalMember" => generalMemberNodeId(reference.Key!, text),
            _ => ReferenceIdentity.NodeId(reference.Kind, text),
        };
        addReference(sourceId, target, reference.Kind == "generalType" ? "generalTypeValue" : kind, path, isDictionaryKey);
    }

    private void scanEnumDependencies(string sourceId, LuaMetadataType type, string? defaultModule,
        string path, HashSet<string> resolving)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (type.Kind == LuaMetadataTypeKind.Enum)
        {
            if (projectEnums.References.TryGetValue(type.Name, out ProjectEnumReference? reference)
                && reference.Kind == "generalMember")
                addReference(sourceId, ReferenceIdentity.NodeId("general", reference.Key!), "generalTypeDependency", path);
            return;
        }
        foreach (LuaMetadataType argument in type.Arguments)
            scanEnumDependencies(sourceId, argument, defaultModule, path, resolving);
        if (type.Kind != LuaMetadataTypeKind.Named)
            return;
        LuaTypeReference typeReference = LuaTypeReference.FromSchema(type).WithDefaultModule(defaultModule);
        if (metadataService.GetType(typeReference) is null || !resolving.Add(typeReference.QualifiedName))
            return;
        try
        {
            ResolvedBlueprintClass structure = classResolver.Resolve(typeReference.QualifiedName);
            foreach (ResolvedBlueprintField field in structure.Fields)
                scanEnumDependencies(sourceId, field.Type.Schema, field.Metadata?.DeclaringType.ModuleName,
                    path + "." + field.Name, resolving);
        }
        finally
        {
            resolving.Remove(typeReference.QualifiedName);
        }
    }

    private void scanEnumSchemaReferences(string sourceId, JsonNode? schema, string path)
    {
        if (schema is JsonObject map)
        {
            if (JsonScalar.String(map["enum"]) is string module
                && projectEnums.References.TryGetValue(module, out ProjectEnumReference? reference)
                && reference.Kind == "generalMember")
                addReference(sourceId, ReferenceIdentity.NodeId("general", reference.Key!), "generalType", path + ".enum");
            foreach (KeyValuePair<string, JsonNode?> pair in map)
                scanEnumSchemaReferences(sourceId, pair.Value, path + "." + pair.Key);
        }
        else if (schema is JsonArray array)
        {
            for (int index = 0; index < array.Count; index++)
                scanEnumSchemaReferences(sourceId, array[index], $"{path}[{index}]");
        }
    }
}
