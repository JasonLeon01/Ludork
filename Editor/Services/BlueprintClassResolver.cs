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
    private readonly Dictionary<string, ResolvedBlueprintTemplate> templateCache = new(StringComparer.Ordinal);
    private long metadataRevision = -1;
    private long revision;
    private bool disposed;

    public BlueprintClassResolver(GameDataService gameData, LuaMetadataService metadataService)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        gameData.Documents.ContentInvalidated += onContentInvalidated;
    }

    public ResolvedBlueprintClass Resolve(string classReference, JsonObject? overrides = null)
    {
        string reference = classReference?.Trim() ?? string.Empty;
        if (templateCache.TryGetValue(reference, out ResolvedBlueprintTemplate? cached))
        {
            if (cached.IsCurrent(metadataService))
                return cached.Materialize(overrides, revision);
            if (cached.MetadataRevision == metadataService.CacheRevision)
                metadataService.ClearCache();
            clearResolutionCaches();
        }

        using IDisposable metadataRead = metadataService.BeginRead();
        ensureMetadataRevision();

        ResolvedBlueprintTemplate template = createCanonicalTemplate(reference);
        if (!template.IsCaptureConsistent)
        {
            metadataService.ClearCache();
            clearResolutionCaches();
            metadataService.RestartDependencyTracking();
            ensureMetadataRevision();
            template = createCanonicalTemplate(reference);
            if (!template.IsCaptureConsistent)
                throw new IOException("Metadata changed while resolving blueprint class");
        }
        templateCache[reference] = template;
        return template.Materialize(overrides, revision);
    }

    public ResolvedBlueprintClass ResolveBlueprint(
        JsonObject blueprint,
        string? blueprintKey = null,
        JsonObject? overrides = null
    )
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureMetadataRevision();
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
        ResolvedBlueprintTemplate template = createBlueprintTemplate(blueprint, reference, key);
        if (!template.IsCaptureConsistent)
        {
            metadataService.ClearCache();
            clearResolutionCaches();
            metadataService.RestartDependencyTracking();
            ensureMetadataRevision();
            template = createBlueprintTemplate(blueprint, reference, key);
            if (!template.IsCaptureConsistent)
                throw new IOException("Metadata changed while resolving blueprint class");
        }
        return template.Materialize(overrides, revision);
    }

    public JsonNode? GetValue(string classReference, string fieldName)
    {
        return Resolve(classReference).GetValue(fieldName);
    }

    public bool IsDerivedFrom(string classReference, string baseTypeName)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        return IsDerivedFrom(Resolve(classReference), baseTypeName);
    }

    public long Revision => revision;

    public bool IsDerivedFrom(ResolvedBlueprintClass resolved, string baseTypeName)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        LuaTypeReference baseType = metadataService.GetRuntimeClassType(baseTypeName)?.Type
            ?? LuaTypeReference.Parse(baseTypeName);
        if (resolved.RootType is not null && metadataService.ResolveMro(resolved.RootType)
            .Any(type => type.Type == baseType))
            return true;
        string? terminalReference = resolved.TerminalReference;
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
        gameData.Documents.ContentInvalidated -= onContentInvalidated;
    }

    public IDisposable BeginBatch()
    {
        return metadataService.BeginRead();
    }

    private ResolvedBlueprintTemplate createCanonicalTemplate(string reference)
    {
        if (reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
        {
            string key = reference[BlueprintPrefix.Length..].Replace('.', '/');
            if (gameData.BlueprintsData.TryGetValue(key, out JsonObject? blueprint))
                return createBlueprintTemplate(blueprint, reference, key);
            return createResolvedTemplate(
                reference,
                null,
                null,
                Array.Empty<LuaTypeReference>(),
                Array.Empty<(string Reference, BlueprintCompatibilityType Type)>(),
                Array.Empty<LuaTypeReference>(),
                Array.Empty<(string Reference, JsonObject Blueprint)>(),
                new HashSet<string>(StringComparer.Ordinal) { key });
        }

        BlueprintRootResolution root = resolveRoot(reference);
        return createResolvedTemplate(
            reference,
            root.TerminalReference,
            root.RootType,
            root.MetadataBases,
            root.CompatibilityTypes,
            root.ProbedMetadataTypes,
            Array.Empty<(string Reference, JsonObject Blueprint)>(),
            new HashSet<string>(StringComparer.Ordinal));
    }

    private ResolvedBlueprintTemplate createBlueprintTemplate(
        JsonObject blueprint,
        string classReference,
        string? blueprintKey
    )
    {
        List<(string Reference, JsonObject Blueprint)> chain = [(classReference, blueprint)];
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
            chain.Add((parent, parentBlueprint));
            parent = getParent(parentBlueprint);
        }

        chain.Reverse();
        BlueprintRootResolution root = resolveRoot(parent);
        return createResolvedTemplate(
            classReference,
            root.TerminalReference,
            root.RootType,
            root.MetadataBases,
            root.CompatibilityTypes,
            root.ProbedMetadataTypes,
            chain,
            visited
        );
    }

    private BlueprintRootResolution resolveRoot(string? reference)
    {
        HashSet<LuaTypeReference> probedMetadataTypes = [];
        if (string.IsNullOrWhiteSpace(reference) || reference.StartsWith(BlueprintPrefix, StringComparison.Ordinal))
            return new BlueprintRootResolution(
                null,
                null,
                Array.Empty<LuaTypeReference>(),
                Array.Empty<(string Reference, BlueprintCompatibilityType Type)>(),
                probedMetadataTypes.ToArray()
            );
        LuaTypeReference originalType = LuaTypeReference.Parse(reference);
        LuaTypeReference? metadataType = findMetadataType(reference, probedMetadataTypes);
        if (metadataType is not null)
            return new BlueprintRootResolution(
                reference,
                metadataType,
                Array.Empty<LuaTypeReference>(),
                Array.Empty<(string Reference, BlueprintCompatibilityType Type)>(),
                probedMetadataTypes.ToArray()
            );

        List<(string Reference, BlueprintCompatibilityType Type)> compatibilityTypes = [];
        HashSet<string> visited = new(StringComparer.Ordinal);
        string? current = originalType.QualifiedName;
        while (!string.IsNullOrWhiteSpace(current) && visited.Add(current)
            && BlueprintCompatibilityCatalog.TryGet(current, out BlueprintCompatibilityType? compatibilityType)
            && compatibilityType is not null)
        {
            compatibilityTypes.Add((current, compatibilityType));
            current = compatibilityType.Parent;
            if (string.IsNullOrWhiteSpace(current))
                break;
            metadataType = findMetadataType(current, probedMetadataTypes);
            if (metadataType is not null)
            {
                compatibilityTypes.Reverse();
                return new BlueprintRootResolution(
                    reference,
                    metadataType,
                    getCompatibilityMetadataBases(compatibilityTypes, probedMetadataTypes),
                    compatibilityTypes,
                    probedMetadataTypes.ToArray()
                );
            }
        }
        compatibilityTypes.Reverse();
        return new BlueprintRootResolution(
            reference,
            originalType,
            getCompatibilityMetadataBases(compatibilityTypes, probedMetadataTypes),
            compatibilityTypes,
            probedMetadataTypes.ToArray()
        );
    }

    private IReadOnlyList<LuaTypeReference> getCompatibilityMetadataBases(
        IReadOnlyList<(string Reference, BlueprintCompatibilityType Type)> compatibilityTypes,
        ISet<LuaTypeReference> probedMetadataTypes
    )
    {
        List<LuaTypeReference> result = [];
        HashSet<string> added = new(StringComparer.Ordinal);
        foreach ((string _, BlueprintCompatibilityType compatibilityType) in compatibilityTypes)
        {
            foreach (string reference in compatibilityType.MetadataBases)
            {
                LuaTypeReference? metadataType = findMetadataType(reference, probedMetadataTypes);
                if (metadataType is not null && added.Add(metadataType.QualifiedName))
                    result.Add(metadataType);
            }
        }
        return result;
    }

    private LuaTypeReference? findMetadataType(
        string reference,
        ISet<LuaTypeReference>? probedMetadataTypes = null)
    {
        LuaTypeReference parsed = LuaTypeReference.Parse(reference);
        probedMetadataTypes?.Add(parsed);
        LuaTypeReference fileClassReference = new(reference, parsed.TypeName);
        probedMetadataTypes?.Add(fileClassReference);
        return metadataService.GetRuntimeClassType(reference)?.Type;
    }

    private ResolvedBlueprintTemplate createResolvedTemplate(
        string classReference,
        string? terminalReference,
        LuaTypeReference? rootType,
        IReadOnlyList<LuaTypeReference> metadataBases,
        IReadOnlyList<(string Reference, BlueprintCompatibilityType Type)> compatibilityTypes,
        IReadOnlyList<LuaTypeReference> probedMetadataTypes,
        IReadOnlyList<(string Reference, JsonObject Blueprint)> blueprintChain,
        IReadOnlySet<string> blueprintDependencies
    )
    {
        List<string> metadataOrder = [];
        Dictionary<string, BlueprintFieldMetadata> schema = new(StringComparer.Ordinal);
        Dictionary<string, string?> fieldSources = new(StringComparer.Ordinal);
        Dictionary<string, JsonNode?> metadataDefaults = new(StringComparer.Ordinal);
        Dictionary<string, JsonNode?> structuralDefaults = new(StringComparer.Ordinal);
        HashSet<string> fieldsWithMetadataDefaults = new(StringComparer.Ordinal);
        JsonObject classMeta = new JsonObject();
        List<string> invalidVars = [];
        HashSet<string> invalidVarSet = new(StringComparer.Ordinal);
        JsonObject rectRangeVars = new JsonObject();
        HashSet<LuaTypeReference> dependencyTypes = new(probedMetadataTypes);
        HashSet<string> dependencyMixins = new(StringComparer.OrdinalIgnoreCase);

        HashSet<LuaTypeReference> mergedMetadataTypes = [];
        foreach (LuaTypeReference metadataBase in metadataBases)
        {
            foreach (LuaTypeMetadata type in metadataService.ResolveMro(metadataBase).Reverse())
            {
                addMetadataDependencies(dependencyTypes, type);
                if (mergedMetadataTypes.Add(type.Type))
                    mergeMetadataType(
                        type,
                        metadataOrder,
                        schema,
                        fieldSources,
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
                addMetadataDependencies(dependencyTypes, type);
                if (mergedMetadataTypes.Add(type.Type))
                    mergeMetadataType(
                        type,
                        metadataOrder,
                        schema,
                        fieldSources,
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
            JsonObject blueprint = blueprintChain[index].Blueprint;
            JsonObject? attrs = blueprint["attrs"] as JsonObject;
            if (index == blueprintChain.Count - 1)
                parentScriptMixin = scriptMixin;
            if (attrs is not null && tryReadBool(attrs["scriptMixin"], out bool localScriptMixin))
                scriptMixin = localScriptMixin;
            string localScriptPath = readString(attrs?["scriptPath"]);
            if (string.IsNullOrWhiteSpace(localScriptPath))
                continue;
            dependencyMixins.Add(localScriptPath);

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
                fieldSources,
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
            JsonObject? structuralDefault = buildStructuredDefault(
                field,
                new HashSet<string>(StringComparer.Ordinal),
                dependencyTypes);
            if (structuralDefault is not null)
                structuralDefaults[name] = structuralDefault;
            if (fieldsWithMetadataDefaults.Contains(name) && structuralDefault is not null)
                metadataDefaults[name] = mergeFieldValue(field.Type.Schema, structuralDefault, metadataDefaults[name]);
        }

        Dictionary<string, JsonNode?> blueprintValues = new(StringComparer.Ordinal);
        List<string> blueprintOrder = [];
        HashSet<string> blueprintFieldSet = new(StringComparer.Ordinal);
        foreach ((string reference, BlueprintCompatibilityType compatibilityType) in compatibilityTypes)
        {
            applyBlueprintValues(
                compatibilityType.Attrs,
                metadataDefaults,
                structuralDefaults,
                blueprintValues,
                blueprintOrder,
                blueprintFieldSet,
                schema,
                fieldSources,
                reference
            );
        }
        foreach ((string reference, JsonObject blueprint) in blueprintChain)
        {
            if (blueprint["attrs"] is not JsonObject attrs)
                continue;
            applyBlueprintValues(
                attrs.Where(pair => scriptMixin || !inactiveMixinNames.Contains(pair.Key)),
                metadataDefaults,
                structuralDefaults,
                blueprintValues,
                blueprintOrder,
                blueprintFieldSet,
                schema,
                fieldSources,
                reference
            );
        }

        LuaMetadataService.DependencySet dependencies = metadataService.CaptureDependencies(
            dependencyTypes,
            dependencyMixins);
        return new ResolvedBlueprintTemplate(
            classReference,
            terminalReference,
            rootType,
            metadataOrder,
            schema,
            fieldSources,
            metadataDefaults,
            fieldsWithMetadataDefaults,
            blueprintOrder,
            blueprintValues,
            blueprintFieldSet,
            classMeta,
            invalidVars,
            rectRangeVars,
            scriptMixin,
            hasBlueprintParent,
            parentScriptMixin,
            localMixinFieldNames,
            scriptMixinError,
            metadataService.CacheRevision,
            dependencies,
            blueprintDependencies
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
        IDictionary<string, string?> fieldSources,
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
            {
                metadataOrder.Add(attr);
                fieldSources[attr] = type.Type.QualifiedName;
            }
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

    private void applyBlueprintValues(
        IEnumerable<KeyValuePair<string, JsonNode?>> attrs,
        IReadOnlyDictionary<string, JsonNode?> metadataDefaults,
        IReadOnlyDictionary<string, JsonNode?> structuralDefaults,
        IDictionary<string, JsonNode?> blueprintValues,
        ICollection<string> blueprintOrder,
        ISet<string> blueprintFieldSet,
        IReadOnlyDictionary<string, BlueprintFieldMetadata> schema,
        IDictionary<string, string?> fieldSources,
        string? sourceClass
    )
    {
        foreach (KeyValuePair<string, JsonNode?> pair in attrs)
        {
            if (blueprintFieldSet.Add(pair.Key))
                blueprintOrder.Add(pair.Key);
            fieldSources.TryAdd(pair.Key, string.IsNullOrWhiteSpace(sourceClass) ? null : sourceClass);
            JsonNode? structureDefault = metadataDefaults.TryGetValue(pair.Key, out JsonNode? metadataDefault)
                ? metadataDefault
                : structuralDefaults.GetValueOrDefault(pair.Key);
            blueprintValues[pair.Key] = structureDefault is JsonObject && pair.Value is JsonObject
                ? mergeFieldValue(schema.GetValueOrDefault(pair.Key)?.Type.Schema, structureDefault, pair.Value)
                : cloneNode(pair.Value);
        }
    }

    private static void addMetadataDependencies(
        ISet<LuaTypeReference> dependencies,
        LuaTypeMetadata metadata)
    {
        dependencies.Add(metadata.Type);
        foreach (LuaTypeReference baseType in metadata.Bases)
            dependencies.Add(baseType.WithDefaultModule(metadata.Type.ModuleName));
    }

    private static ResolvedBlueprintField createUnknownField(
        string name,
        JsonNode? value,
        JsonNode? blueprintDefaultValue,
        bool hasBlueprintDefaultValue,
        string? sourceClass
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
            hasBlueprintDefaultValue,
            sourceClass
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
        HashSet<string> resolving,
        ISet<LuaTypeReference> dependencyTypes
    )
    {
        LuaTypeReference typeReference = field.Type.WithDefaultModule(field.DeclaringType.ModuleName);
        dependencyTypes.Add(typeReference);
        if (!resolving.Add(typeReference.QualifiedName))
            return null;
        if (metadataService.GetType(typeReference) is null)
        {
            resolving.Remove(typeReference.QualifiedName);
            return null;
        }
        IReadOnlyList<LuaTypeMetadata> mro = metadataService.ResolveMro(typeReference);
        foreach (LuaTypeMetadata type in mro)
            addMetadataDependencies(dependencyTypes, type);
        JsonObject result = new JsonObject();
        bool hasValue = false;
        foreach (LuaTypeMetadata type in mro.Reverse())
        {
            foreach (string name in type.Attrs)
            {
                if (!type.Fields.TryGetValue(name, out BlueprintFieldMetadata? nestedField))
                    continue;
                JsonObject? nestedDefault = buildStructuredDefault(nestedField, resolving, dependencyTypes);
                if (nestedField.HasDefaultValue)
                {
                    result[name] = mergeFieldValue(nestedField.Type.Schema, nestedDefault, nestedField.DefaultValue);
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

    private JsonNode? mergeFieldValue(LuaMetadataType? type, JsonNode? inheritedValue, JsonNode? nextValue)
    {
        if (type?.Kind == LuaMetadataTypeKind.Union)
            return cloneNode(nextValue);
        if (inheritedValue is not JsonObject inheritedObject || nextValue is not JsonObject nextObject)
            return cloneNode(nextValue);
        IReadOnlyList<LuaTypeMetadata> metadata = type?.Kind == LuaMetadataTypeKind.Named
            && metadataService.GetType(type.Name) is not null ? metadataService.ResolveMro(type.Name) : [];
        JsonObject result = (JsonObject)inheritedObject.DeepClone();
        foreach (KeyValuePair<string, JsonNode?> pair in nextObject)
        {
            LuaMetadataType? childType = type?.Kind == LuaMetadataTypeKind.Dictionary
                ? type.Arguments[1]
                : metadata.Select(owner => owner.Fields.GetValueOrDefault(pair.Key)).FirstOrDefault(field => field is not null)?.Type.Schema;
            result.TryGetPropertyValue(pair.Key, out JsonNode? previous);
            result[pair.Key] = mergeFieldValue(childType, previous, pair.Value);
        }
        return result;
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

    private void ensureMetadataRevision()
    {
        long revision = metadataService.CacheRevision;
        if (metadataRevision == revision)
            return;
        metadataRevision = revision;
        clearResolutionCaches();
    }

    private void clearResolutionCaches()
    {
        templateCache.Clear();
        revision++;
    }

    private void onContentInvalidated(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset)
        {
            metadataService.ClearCache();
            clearResolutionCaches();
            metadataRevision = metadataService.CacheRevision;
            return;
        }
        HashSet<string> changedKeys = new(StringComparer.Ordinal);
        foreach (EditorDocumentChange change in args.Changes)
        {
            if (change.Section != "Blueprints" || !change.ContentChanged && !change.IdentityChanged)
                continue;
            if (change.PreviousKey is string previousKey)
                changedKeys.Add(previousKey);
            if (change.Key is string key)
                changedKeys.Add(key);
        }
        if (changedKeys.Count == 0)
            return;
        foreach (string reference in templateCache.Where(pair => pair.Value.DependsOn(changedKeys))
                     .Select(pair => pair.Key).ToArray())
            templateCache.Remove(reference);
        revision++;
    }

    private sealed record BlueprintRootResolution(
        string? TerminalReference,
        LuaTypeReference? RootType,
        IReadOnlyList<LuaTypeReference> MetadataBases,
        IReadOnlyList<(string Reference, BlueprintCompatibilityType Type)> CompatibilityTypes,
        IReadOnlyList<LuaTypeReference> ProbedMetadataTypes
    );

    private sealed class ResolvedBlueprintTemplate
    {
        private readonly string classReference;
        private readonly string? terminalReference;
        private readonly LuaTypeReference? rootType;
        private readonly IReadOnlyList<string> metadataOrder;
        private readonly IReadOnlyDictionary<string, BlueprintFieldMetadata> schema;
        private readonly IReadOnlyDictionary<string, string?> fieldSources;
        private readonly IReadOnlyDictionary<string, JsonNode?> metadataDefaults;
        private readonly IReadOnlySet<string> fieldsWithMetadataDefaults;
        private readonly IReadOnlyList<string> blueprintOrder;
        private readonly IReadOnlyDictionary<string, JsonNode?> blueprintValues;
        private readonly IReadOnlySet<string> blueprintFieldSet;
        private readonly JsonObject classMeta;
        private readonly IReadOnlyList<string> invalidVars;
        private readonly JsonObject rectRangeVars;
        private readonly bool scriptMixin;
        private readonly bool hasBlueprintParent;
        private readonly bool parentScriptMixin;
        private readonly IReadOnlyList<string> localMixinFieldNames;
        private readonly string? scriptMixinError;
        private readonly LuaMetadataService.DependencySet dependencies;
        private readonly IReadOnlySet<string> blueprintDependencies;

        public ResolvedBlueprintTemplate(
            string classReference,
            string? terminalReference,
            LuaTypeReference? rootType,
            IReadOnlyList<string> metadataOrder,
            IReadOnlyDictionary<string, BlueprintFieldMetadata> schema,
            IReadOnlyDictionary<string, string?> fieldSources,
            IReadOnlyDictionary<string, JsonNode?> metadataDefaults,
            IReadOnlySet<string> fieldsWithMetadataDefaults,
            IReadOnlyList<string> blueprintOrder,
            IReadOnlyDictionary<string, JsonNode?> blueprintValues,
            IReadOnlySet<string> blueprintFieldSet,
            JsonObject classMeta,
            IReadOnlyList<string> invalidVars,
            JsonObject rectRangeVars,
            bool scriptMixin,
            bool hasBlueprintParent,
            bool parentScriptMixin,
            IReadOnlyList<string> localMixinFieldNames,
            string? scriptMixinError,
            long metadataRevision,
            LuaMetadataService.DependencySet dependencies,
            IReadOnlySet<string> blueprintDependencies)
        {
            this.classReference = classReference;
            this.terminalReference = terminalReference;
            this.rootType = rootType;
            this.metadataOrder = metadataOrder.ToArray();
            this.schema = schema;
            this.fieldSources = fieldSources;
            this.metadataDefaults = metadataDefaults;
            this.fieldsWithMetadataDefaults = new HashSet<string>(fieldsWithMetadataDefaults, StringComparer.Ordinal);
            this.blueprintOrder = blueprintOrder.ToArray();
            this.blueprintValues = blueprintValues;
            this.blueprintFieldSet = new HashSet<string>(blueprintFieldSet, StringComparer.Ordinal);
            this.classMeta = classMeta;
            this.invalidVars = invalidVars.ToArray();
            this.rectRangeVars = rectRangeVars;
            this.scriptMixin = scriptMixin;
            this.hasBlueprintParent = hasBlueprintParent;
            this.parentScriptMixin = parentScriptMixin;
            this.localMixinFieldNames = localMixinFieldNames.ToArray();
            this.scriptMixinError = scriptMixinError;
            MetadataRevision = metadataRevision;
            this.dependencies = dependencies;
            this.blueprintDependencies = new HashSet<string>(blueprintDependencies, StringComparer.Ordinal);
        }

        public long MetadataRevision { get; }
        public bool IsCaptureConsistent => dependencies.IsConsistent;

        public bool DependsOn(IReadOnlySet<string> keys) => blueprintDependencies.Any(keys.Contains);

        public bool IsCurrent(LuaMetadataService metadataService)
        {
            return MetadataRevision == metadataService.CacheRevision
                && metadataService.AreDependenciesCurrent(dependencies);
        }

        public ResolvedBlueprintClass Materialize(JsonObject? overrides, long resolverRevision)
        {
            List<ResolvedBlueprintField> fields = [];
            HashSet<string> added = new(StringComparer.Ordinal);
            foreach (string name in metadataOrder)
            {
                BlueprintFieldMetadata fieldMetadata = schema[name];
                bool hasBlueprintValue = blueprintFieldSet.Contains(name);
                bool hasDefault = fieldsWithMetadataDefaults.Contains(name);
                bool hasOverride = overrides?.ContainsKey(name) == true;
                if (!hasDefault && !hasBlueprintValue && !hasOverride && !fieldMetadata.Component)
                    continue;
                JsonNode? blueprintDefaultValue = hasBlueprintValue
                    ? blueprintValues[name]
                    : hasDefault
                        ? metadataDefaults[name]
                        : null;
                JsonNode? value = hasOverride ? overrides![name] : blueprintDefaultValue;
                fields.Add(new ResolvedBlueprintField(
                    name,
                    fieldMetadata.Type,
                    value,
                    blueprintDefaultValue,
                    fieldMetadata,
                    false,
                    hasBlueprintValue || hasDefault,
                    fieldSources.GetValueOrDefault(name)));
                added.Add(name);
            }

            foreach (string name in blueprintOrder)
            {
                if (added.Contains(name))
                    continue;
                JsonNode? blueprintDefaultValue = blueprintValues[name];
                JsonNode? value = overrides?.ContainsKey(name) == true
                    ? overrides[name]
                    : blueprintDefaultValue;
                if (schema.TryGetValue(name, out BlueprintFieldMetadata? fieldMetadata))
                {
                    fields.Add(new ResolvedBlueprintField(
                        name,
                        fieldMetadata.Type,
                        value,
                        blueprintDefaultValue,
                        fieldMetadata,
                        false,
                        true,
                        fieldSources.GetValueOrDefault(name)));
                }
                else
                {
                    fields.Add(createUnknownField(name, value, blueprintDefaultValue, true, fieldSources.GetValueOrDefault(name)));
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
                            pair.Value,
                            null,
                            fieldMetadata,
                            false,
                            false,
                            fieldSources.GetValueOrDefault(pair.Key)));
                    }
                    else
                    {
                        fields.Add(createUnknownField(pair.Key, pair.Value, null, false, null));
                    }
                    added.Add(pair.Key);
                }
            }

            return new ResolvedBlueprintClass(
                classReference,
                terminalReference,
                rootType,
                fields,
                classMeta,
                invalidVars,
                rectRangeVars,
                scriptMixin,
                hasBlueprintParent,
                parentScriptMixin,
                metadataOrder,
                localMixinFieldNames,
                scriptMixinError,
                resolverRevision,
                MetadataRevision,
                blueprintDependencies);
        }

    }
}
