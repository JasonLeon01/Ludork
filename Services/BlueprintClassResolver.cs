using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using MoonSharp.Interpreter;

namespace Ludork.Services;

public sealed class BlueprintClassResolver : IDisposable
{
    private const string BlueprintPrefix = "Data.Blueprints.";
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private bool disposed;

    public BlueprintClassResolver(GameDataService gameData, LuaMetadataService metadataService)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        gameData.DataReloaded += onDataReloaded;
    }

    public ResolvedBlueprintClass Resolve(string classReference, JsonObject? overrides = null)
    {
        string reference = classReference?.Trim() ?? string.Empty;
        if (reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            string key = reference[BlueprintPrefix.Length..].Replace('.', '/');
            if (gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
                return resolveBlueprint(blueprint, reference, key, overrides);
            return createResolvedClass(
                reference,
                null,
                Array.Empty<LuaTypeReference>(),
                Array.Empty<BlueprintCompatibilityType>(),
                Array.Empty<JsonObject>(),
                overrides
            );
        }

        BlueprintRootResolution root = resolveRoot(reference);
        return createResolvedClass(
            reference,
            root.RootType,
            root.MetadataBases,
            root.CompatibilityTypes,
            Array.Empty<JsonObject>(),
            overrides
        );
    }

    public ResolvedBlueprintClass ResolveBlueprint(
        JsonObject blueprint,
        string? blueprintKey = null,
        JsonObject? overrides = null
    )
    {
        string reference = string.IsNullOrWhiteSpace(blueprintKey)
            ? string.Empty
            : blueprintKey.StartsWith(BlueprintPrefix, StringComparison.Ordinal)
                ? blueprintKey
                : BlueprintPrefix + blueprintKey.Replace('/', '.').Replace('\\', '.');
        string? key = string.IsNullOrWhiteSpace(blueprintKey)
            ? null
            : blueprintKey.StartsWith(BlueprintPrefix, StringComparison.Ordinal)
                ? blueprintKey[BlueprintPrefix.Length..].Replace('.', '/')
                : blueprintKey.Replace('\\', '/');
        return resolveBlueprint(blueprint, reference, key, overrides);
    }

    public JsonNode? GetValue(string classReference, string fieldName)
    {
        return Resolve(classReference).GetValue(fieldName);
    }

    public bool IsDerivedFrom(string classReference, string baseTypeName)
    {
        ResolvedBlueprintClass resolved = Resolve(classReference);
        LuaTypeReference baseType = LuaTypeReference.Parse(baseTypeName);
        if (resolved.RootType is not null && metadataService.ResolveMro(resolved.RootType)
            .Any(type => type.Type == baseType))
            return true;
        string? terminalReference = getTerminalReference(classReference);
        if (string.IsNullOrWhiteSpace(terminalReference) || findMetadataType(terminalReference) is not null)
            return false;
        return BlueprintCompatibilityCatalog.IsDerivedFrom(
            LuaTypeReference.Parse(terminalReference).QualifiedName,
            baseType.QualifiedName
        );
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.DataReloaded -= onDataReloaded;
    }

    private ResolvedBlueprintClass resolveBlueprint(
        JsonObject blueprint,
        string classReference,
        string? blueprintKey,
        JsonObject? overrides
    )
    {
        List<JsonObject> chain = [blueprint];
        HashSet<string> visited = new(StringComparer.Ordinal);
        if (!string.IsNullOrWhiteSpace(blueprintKey))
            visited.Add(blueprintKey);
        string? parent = getParent(blueprint);
        while (!string.IsNullOrWhiteSpace(parent) && parent.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            string key = parent[BlueprintPrefix.Length..].Replace('.', '/');
            if (!visited.Add(key) || !gameData.BlueprintsData.TryGetValue(key, out JsonObject? parentBlueprint))
            {
                parent = null;
                break;
            }
            chain.Add(parentBlueprint);
            parent = getParent(parentBlueprint);
        }

        chain.Reverse();
        BlueprintRootResolution root = resolveRoot(parent);
        return createResolvedClass(
            classReference,
            root.RootType,
            root.MetadataBases,
            root.CompatibilityTypes,
            chain,
            overrides
        );
    }

    private BlueprintRootResolution resolveRoot(string? reference)
    {
        if (string.IsNullOrWhiteSpace(reference) || reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            return new BlueprintRootResolution(
                null,
                Array.Empty<LuaTypeReference>(),
                Array.Empty<BlueprintCompatibilityType>()
            );
        LuaTypeReference originalType = LuaTypeReference.Parse(reference);
        LuaTypeReference? metadataType = findMetadataType(reference);
        if (metadataType is not null)
            return new BlueprintRootResolution(
                metadataType,
                Array.Empty<LuaTypeReference>(),
                Array.Empty<BlueprintCompatibilityType>()
            );

        List<BlueprintCompatibilityType> compatibilityTypes = [];
        HashSet<string> visited = new(StringComparer.Ordinal);
        string? current = originalType.QualifiedName;
        while (!string.IsNullOrWhiteSpace(current) && visited.Add(current)
            && BlueprintCompatibilityCatalog.TryGet(current, out BlueprintCompatibilityType? compatibilityType)
            && compatibilityType is not null)
        {
            compatibilityTypes.Add(compatibilityType);
            current = compatibilityType.Parent;
            if (string.IsNullOrWhiteSpace(current))
                break;
            metadataType = findMetadataType(current);
            if (metadataType is not null)
            {
                compatibilityTypes.Reverse();
                return new BlueprintRootResolution(
                    metadataType,
                    getCompatibilityMetadataBases(compatibilityTypes),
                    compatibilityTypes
                );
            }
        }
        compatibilityTypes.Reverse();
        return new BlueprintRootResolution(
            originalType,
            getCompatibilityMetadataBases(compatibilityTypes),
            compatibilityTypes
        );
    }

    private IReadOnlyList<LuaTypeReference> getCompatibilityMetadataBases(
        IReadOnlyList<BlueprintCompatibilityType> compatibilityTypes
    )
    {
        List<LuaTypeReference> result = [];
        HashSet<string> added = new(StringComparer.Ordinal);
        foreach (BlueprintCompatibilityType compatibilityType in compatibilityTypes)
        {
            foreach (string reference in compatibilityType.MetadataBases)
            {
                LuaTypeReference? metadataType = findMetadataType(reference);
                if (metadataType is not null && added.Add(metadataType.QualifiedName))
                    result.Add(metadataType);
            }
        }
        return result;
    }

    private LuaTypeReference? findMetadataType(string reference)
    {
        LuaTypeReference parsed = LuaTypeReference.Parse(reference);
        if (metadataService.GetType(parsed) is not null)
            return parsed;
        LuaTypeReference fileClassReference = new(reference, parsed.TypeName);
        return metadataService.GetType(fileClassReference) is not null ? fileClassReference : null;
    }

    private ResolvedBlueprintClass createResolvedClass(
        string classReference,
        LuaTypeReference? rootType,
        IReadOnlyList<LuaTypeReference> metadataBases,
        IReadOnlyList<BlueprintCompatibilityType> compatibilityTypes,
        IReadOnlyList<JsonObject> blueprintChain,
        JsonObject? overrides
    )
    {
        List<string> metadataOrder = [];
        Dictionary<string, BlueprintFieldMetadata> schema = new(StringComparer.Ordinal);
        Dictionary<string, JsonNode?> metadataDefaults = new(StringComparer.Ordinal);
        Dictionary<string, JsonNode?> structuralDefaults = new(StringComparer.Ordinal);
        HashSet<string> fieldsWithMetadataDefaults = new(StringComparer.Ordinal);
        JsonObject classMeta = new JsonObject();
        List<string> invalidVars = [];
        HashSet<string> invalidVarSet = new(StringComparer.Ordinal);
        JsonObject rectRangeVars = new JsonObject();

        HashSet<LuaTypeReference> mergedMetadataTypes = [];
        foreach (LuaTypeReference metadataBase in metadataBases)
        {
            foreach (LuaTypeMetadata type in metadataService.ResolveMro(metadataBase).Reverse())
            {
                if (mergedMetadataTypes.Add(type.Type))
                    mergeMetadataType(
                        type,
                        metadataOrder,
                        schema,
                        metadataDefaults,
                        fieldsWithMetadataDefaults,
                        classMeta,
                        invalidVars,
                        invalidVarSet,
                        rectRangeVars
                    );
            }
        }
        if (rootType is not null)
        {
            foreach (LuaTypeMetadata type in metadataService.ResolveMro(rootType).Reverse())
            {
                if (mergedMetadataTypes.Add(type.Type))
                    mergeMetadataType(
                        type,
                        metadataOrder,
                        schema,
                        metadataDefaults,
                        fieldsWithMetadataDefaults,
                        classMeta,
                        invalidVars,
                        invalidVarSet,
                        rectRangeVars
                    );
            }
        }

        HashSet<string> rootSchemaNames = new(schema.Keys, StringComparer.Ordinal);
        HashSet<string> inactiveMixinNames = new(StringComparer.Ordinal);
        List<string> localMixinFieldNames = [];
        bool scriptMixin = false;
        bool parentScriptMixin = false;
        bool hasBlueprintParent = blueprintChain.Count > 1;
        string? scriptMixinError = null;
        for (int index = 0; index < blueprintChain.Count; index++)
        {
            JsonObject blueprint = blueprintChain[index];
            JsonObject? attrs = blueprint["attrs"] as JsonObject;
            if (index == blueprintChain.Count - 1)
                parentScriptMixin = scriptMixin;
            if (attrs is not null && tryReadBool(attrs["scriptMixin"], out bool localScriptMixin))
                scriptMixin = localScriptMixin;
            string localScriptPath = readString(attrs?["scriptPath"]);
            if (string.IsNullOrWhiteSpace(localScriptPath))
                continue;

            LuaTypeMetadata? mixinMetadata = null;
            try
            {
                mixinMetadata = metadataService.LoadScriptMixinMetadata(localScriptPath);
            }
            catch (InterpreterException exception)
            {
                scriptMixinError = exception.DecoratedMessage ?? exception.Message;
            }
            catch (InvalidDataException exception)
            {
                scriptMixinError = exception.Message;
            }
            catch (IOException exception)
            {
                scriptMixinError = exception.Message;
            }
            catch (UnauthorizedAccessException exception)
            {
                scriptMixinError = exception.Message;
            }
            if (mixinMetadata is null)
                continue;
            if (index == blueprintChain.Count - 1)
                localMixinFieldNames.AddRange(mixinMetadata.Attrs);
            if (!scriptMixin)
            {
                foreach (string name in mixinMetadata.Attrs)
                    inactiveMixinNames.Add(name);
                continue;
            }
            mergeMetadataType(
                mixinMetadata,
                metadataOrder,
                schema,
                metadataDefaults,
                fieldsWithMetadataDefaults,
                classMeta,
                invalidVars,
                invalidVarSet,
                rectRangeVars
            );
        }

        inactiveMixinNames.ExceptWith(rootSchemaNames);
        foreach (string name in metadataOrder)
        {
            BlueprintFieldMetadata field = schema[name];
            JsonObject? structuralDefault = buildStructuredDefault(field, new HashSet<string>(StringComparer.Ordinal));
            if (structuralDefault is not null)
                structuralDefaults[name] = structuralDefault;
            if (fieldsWithMetadataDefaults.Contains(name) && structuralDefault is not null)
                metadataDefaults[name] = mergeNodes(structuralDefault, metadataDefaults[name]);
        }

        Dictionary<string, JsonNode?> blueprintValues = new(StringComparer.Ordinal);
        List<string> blueprintOrder = [];
        HashSet<string> blueprintFieldSet = new(StringComparer.Ordinal);
        foreach (BlueprintCompatibilityType compatibilityType in compatibilityTypes)
        {
            applyBlueprintValues(
                compatibilityType.Attrs,
                metadataDefaults,
                structuralDefaults,
                blueprintValues,
                blueprintOrder,
                blueprintFieldSet
            );
        }
        foreach (JsonObject blueprint in blueprintChain)
        {
            if (blueprint["attrs"] is not JsonObject attrs)
                continue;
            applyBlueprintValues(
                attrs.Where(pair => scriptMixin || !inactiveMixinNames.Contains(pair.Key)),
                metadataDefaults,
                structuralDefaults,
                blueprintValues,
                blueprintOrder,
                blueprintFieldSet
            );
        }

        List<ResolvedBlueprintField> fields = [];
        HashSet<string> added = new(StringComparer.Ordinal);
        foreach (string name in metadataOrder)
        {
            bool hasBlueprintValue = blueprintFieldSet.Contains(name);
            bool hasDefault = fieldsWithMetadataDefaults.Contains(name);
            bool hasOverride = overrides?.ContainsKey(name) == true;
            if (!hasDefault && !hasBlueprintValue && !hasOverride)
                continue;
            JsonNode? blueprintDefaultValue = hasBlueprintValue
                ? cloneNode(blueprintValues[name])
                : hasDefault
                    ? cloneNode(metadataDefaults[name])
                    : null;
            JsonNode? value = hasOverride
                ? cloneNode(overrides![name])
                : cloneNode(blueprintDefaultValue);
            BlueprintFieldMetadata fieldMetadata = schema[name];
            fields.Add(new ResolvedBlueprintField(
                name,
                fieldMetadata.Type,
                value,
                blueprintDefaultValue,
                fieldMetadata,
                false,
                hasBlueprintValue || hasDefault
            ));
            added.Add(name);
        }

        foreach (string name in blueprintOrder)
        {
            if (added.Contains(name))
                continue;
            JsonNode? blueprintDefaultValue = cloneNode(blueprintValues[name]);
            JsonNode? value = overrides?.ContainsKey(name) == true
                ? cloneNode(overrides[name])
                : cloneNode(blueprintDefaultValue);
            if (schema.TryGetValue(name, out BlueprintFieldMetadata? fieldMetadata))
            {
                fields.Add(new ResolvedBlueprintField(
                    name,
                    fieldMetadata.Type,
                    value,
                    blueprintDefaultValue,
                    fieldMetadata,
                    false,
                    true
                ));
            }
            else
            {
                fields.Add(createUnknownField(name, value, blueprintDefaultValue, true));
            }
            added.Add(name);
        }

        if (overrides is not null)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in overrides)
            {
                if (added.Contains(pair.Key))
                    continue;
                if (schema.TryGetValue(pair.Key, out BlueprintFieldMetadata? fieldMetadata))
                {
                    fields.Add(new ResolvedBlueprintField(
                        pair.Key,
                        fieldMetadata.Type,
                        cloneNode(pair.Value),
                        null,
                        fieldMetadata,
                        false,
                        false
                    ));
                }
                else
                {
                    fields.Add(createUnknownField(pair.Key, pair.Value, null, false));
                }
                added.Add(pair.Key);
            }
        }

        return new ResolvedBlueprintClass(
            classReference,
            rootType,
            fields,
            classMeta,
            invalidVars,
            rectRangeVars,
            scriptMixin,
            hasBlueprintParent,
            parentScriptMixin,
            metadataOrder.ToArray(),
            localMixinFieldNames,
            scriptMixinError
        );
    }

    private static bool tryReadBool(JsonNode? value, out bool result)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue<bool>(out result))
                return true;
        }
        result = false;
        return false;
    }

    private static string readString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue<string>(out string? result)
            ? result.Trim()
            : string.Empty;
    }

    private static void mergeMetadataType(
        LuaTypeMetadata type,
        ICollection<string> metadataOrder,
        IDictionary<string, BlueprintFieldMetadata> schema,
        IDictionary<string, JsonNode?> metadataDefaults,
        ISet<string> fieldsWithMetadataDefaults,
        JsonObject classMeta,
        ICollection<string> invalidVars,
        ISet<string> invalidVarSet,
        JsonObject rectRangeVars
    )
    {
        mergeObject(classMeta, type.Meta);
        mergeObject(rectRangeVars, type.RectRangeVars);
        foreach (string invalidVar in type.InvalidVars)
        {
            if (invalidVarSet.Add(invalidVar))
                invalidVars.Add(invalidVar);
        }
        foreach (string attr in type.Attrs)
        {
            if (!type.Fields.TryGetValue(attr, out BlueprintFieldMetadata? field))
                continue;
            if (!schema.ContainsKey(attr))
                metadataOrder.Add(attr);
            schema[attr] = schema.TryGetValue(attr, out BlueprintFieldMetadata? inheritedField)
                ? mergeFieldMetadata(inheritedField, field)
                : field;
            if (field.HasDefaultValue)
            {
                fieldsWithMetadataDefaults.Add(attr);
                metadataDefaults[attr] = cloneNode(field.DefaultValue);
            }
        }
    }

    private static void applyBlueprintValues(
        IEnumerable<KeyValuePair<string, JsonNode?>> attrs,
        IReadOnlyDictionary<string, JsonNode?> metadataDefaults,
        IReadOnlyDictionary<string, JsonNode?> structuralDefaults,
        IDictionary<string, JsonNode?> blueprintValues,
        ICollection<string> blueprintOrder,
        ISet<string> blueprintFieldSet
    )
    {
        foreach (KeyValuePair<string, JsonNode?> pair in attrs)
        {
            if (blueprintFieldSet.Add(pair.Key))
                blueprintOrder.Add(pair.Key);
            JsonNode? structureDefault = metadataDefaults.TryGetValue(pair.Key, out JsonNode? metadataDefault)
                ? metadataDefault
                : structuralDefaults.GetValueOrDefault(pair.Key);
            blueprintValues[pair.Key] = structureDefault is JsonObject && pair.Value is JsonObject
                ? mergeNodes(structureDefault, pair.Value)
                : cloneNode(pair.Value);
        }
    }

    private static ResolvedBlueprintField createUnknownField(
        string name,
        JsonNode? value,
        JsonNode? blueprintDefaultValue,
        bool hasBlueprintDefaultValue
    )
    {
        LuaTypeReference inferredType = inferType(value ?? blueprintDefaultValue);
        return new ResolvedBlueprintField(
            name,
            inferredType,
            value,
            blueprintDefaultValue,
            null,
            true,
            hasBlueprintDefaultValue
        );
    }

    private static LuaTypeReference inferType(JsonNode? value)
    {
        if (value is JsonObject)
            return new LuaTypeReference(null, "table");
        if (value is JsonArray array)
        {
            List<LuaTypeReference> elementTypes = array
                .Where(item => item is not null)
                .Select(inferType)
                .ToList();
            if (elementTypes.Count == 0)
                return new LuaTypeReference(null, "any[]");
            bool onlyNumbers = elementTypes.All(type => type.TypeName is "int" or "float");
            string elementType = onlyNumbers && elementTypes.Any(type => type.TypeName == "float")
                ? "float"
                : elementTypes.All(type => type == elementTypes[0])
                    ? elementTypes[0].QualifiedName
                    : "any";
            return new LuaTypeReference(null, elementType + "[]");
        }
        if (value is not JsonValue scalar)
            return new LuaTypeReference(null, "any");
        if (scalar.TryGetValue<bool>(out bool _))
            return new LuaTypeReference(null, "bool");
        if (scalar.TryGetValue<string>(out string? _))
            return new LuaTypeReference(null, "string");
        if (scalar.TryGetValue<int>(out int _) || scalar.TryGetValue<long>(out long _))
            return new LuaTypeReference(null, "int");
        if (scalar.TryGetValue<float>(out float _) || scalar.TryGetValue<double>(out double _)
            || scalar.TryGetValue<decimal>(out decimal _))
            return new LuaTypeReference(null, "float");
        return new LuaTypeReference(null, "any");
    }

    private static void mergeObject(JsonObject target, JsonObject source)
    {
        foreach (KeyValuePair<string, JsonNode?> pair in source)
        {
            target.TryGetPropertyValue(pair.Key, out JsonNode? inheritedValue);
            target[pair.Key] = mergeNodes(inheritedValue, pair.Value);
        }
    }

    private static BlueprintFieldMetadata mergeFieldMetadata(
        BlueprintFieldMetadata inherited,
        BlueprintFieldMetadata derived)
    {
        JsonObject meta = (JsonObject)inherited.Meta.DeepClone();
        mergeObject(meta, derived.Meta);
        bool hasDefault = derived.HasDefaultValue || inherited.HasDefaultValue;
        JsonNode? defaultValue = derived.HasDefaultValue
            ? derived.DefaultValue
            : inherited.DefaultValue;
        return new BlueprintFieldMetadata(
            derived.Name,
            derived.Type,
            hasDefault,
            defaultValue,
            derived.Component || inherited.Component,
            meta,
            derived.DeclaringType);
    }

    private JsonObject? buildStructuredDefault(
        BlueprintFieldMetadata field,
        HashSet<string> resolving
    )
    {
        LuaTypeReference typeReference = field.Type.WithDefaultModule(field.DeclaringType.ModuleName);
        if (!resolving.Add(typeReference.QualifiedName))
            return null;
        if (metadataService.GetType(typeReference) is null)
        {
            resolving.Remove(typeReference.QualifiedName);
            return null;
        }
        IReadOnlyList<LuaTypeMetadata> mro = metadataService.ResolveMro(typeReference);
        JsonObject result = new JsonObject();
        bool hasValue = false;
        foreach (LuaTypeMetadata type in mro.Reverse())
        {
            foreach (string name in type.Attrs)
            {
                if (!type.Fields.TryGetValue(name, out BlueprintFieldMetadata? nestedField))
                    continue;
                JsonObject? nestedDefault = buildStructuredDefault(nestedField, resolving);
                if (nestedField.HasDefaultValue)
                {
                    result[name] = mergeNodes(nestedDefault, nestedField.DefaultValue);
                    hasValue = true;
                }
                else if (nestedDefault is not null)
                {
                    result[name] = nestedDefault;
                    hasValue = true;
                }
            }
        }
        resolving.Remove(typeReference.QualifiedName);
        return hasValue ? result : null;
    }

    private static JsonNode? mergeNodes(JsonNode? inheritedValue, JsonNode? nextValue)
    {
        if (inheritedValue is JsonObject inheritedObject && nextValue is JsonObject nextObject)
        {
            JsonObject result = (JsonObject)inheritedObject.DeepClone();
            foreach (KeyValuePair<string, JsonNode?> pair in nextObject)
            {
                result.TryGetPropertyValue(pair.Key, out JsonNode? currentValue);
                result[pair.Key] = mergeNodes(currentValue, pair.Value);
            }
            return result;
        }
        return cloneNode(nextValue);
    }

    private static JsonNode? cloneNode(JsonNode? value)
    {
        return value?.DeepClone();
    }

    private static string? getParent(JsonObject blueprint)
    {
        return blueprint["parent"]?.GetValue<string>()?.Trim();
    }

    private string? getTerminalReference(string classReference)
    {
        string reference = classReference?.Trim() ?? string.Empty;
        if (!reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            return string.IsNullOrWhiteSpace(reference) ? null : reference;
        string key = reference[BlueprintPrefix.Length..].Replace('.', '/');
        HashSet<string> visited = new(StringComparer.Ordinal);
        while (visited.Add(key) && gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
        {
            string? parent = getParent(blueprint);
            if (string.IsNullOrWhiteSpace(parent))
                return null;
            if (!parent.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
                return parent;
            key = parent[BlueprintPrefix.Length..].Replace('.', '/');
        }
        return null;
    }

    private void onDataReloaded(object? sender, EventArgs e)
    {
        metadataService.ClearCache();
    }

    private sealed record BlueprintRootResolution(
        LuaTypeReference? RootType,
        IReadOnlyList<LuaTypeReference> MetadataBases,
        IReadOnlyList<BlueprintCompatibilityType> CompatibilityTypes
    );
}
