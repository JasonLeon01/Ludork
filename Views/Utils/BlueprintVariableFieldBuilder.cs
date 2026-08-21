using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

public sealed class BlueprintVariableFieldBuilder
{
    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;

    public BlueprintVariableFieldBuilder(
        GameDataService gameData,
        LuaMetadataService metadataService)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
    }

    public IReadOnlyList<BlueprintVariableField> Build(
        ResolvedBlueprintClass resolved,
        bool readOnlyGeneralDataFields = false)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        HashSet<string> invalidVars = new(resolved.InvalidVars, StringComparer.Ordinal);
        GeneralDataFieldSource? generalDataFields = readOnlyGeneralDataFields
            ? getGeneralDataFields(resolved)
            : null;
        List<BlueprintVariableField> result = [];
        foreach (ResolvedBlueprintField field in resolved.Fields)
        {
            if (invalidVars.Contains(field.Name)
                || field.IsUnknown && !field.HasBlueprintDefaultValue
                || !field.HasBlueprintDefaultValue && field.Metadata?.Component != true)
            {
                continue;
            }
            string? rectSource = getString(resolved.RectRangeVars[field.Name]);
            bool isGeneralDataField = generalDataFields?.Names.Contains(field.Name) == true;
            JsonNode? generalDataValue = isGeneralDataField
                ? getGeneralDataValue(generalDataFields, field.Name)
                : null;
            result.Add(createFormField(
                field,
                field.BlueprintDefaultValue,
                field.HasBlueprintDefaultValue,
                rectSource,
                new HashSet<string>(StringComparer.Ordinal),
                field.Name == "scriptMixin" && resolved.HasBlueprintParent
                    || isGeneralDataField,
                generalDataValue,
                field.Metadata?.Component == true ? generalDataFields : null));
        }
        return result;
    }

    public bool IsGeneralDataSelector(ResolvedBlueprintClass resolved, string fieldName)
    {
        if (!string.Equals(fieldName, "ID", StringComparison.Ordinal))
            return false;
        ResolvedBlueprintField? field = resolved.GetField(fieldName);
        string? dataType = getGeneralDataType(field?.Metadata?.Meta["GeneralDataVars"]);
        return dataType is not null && gameData.GeneralData.ContainsKey(dataType);
    }

    public BlueprintVariableField BuildNodeParameter(BlueprintGraphPort port)
    {
        LuaMetadataType type = LuaMetadataType.Parse(port.TypeName);
        LuaTypeReference? namedReference = type.Kind == LuaMetadataTypeKind.Named
            ? LuaTypeReference.Parse(type.Name)
            : null;
        JsonObject meta = port.Meta.DeepClone() as JsonObject ?? [];
        string? assetSubdirectory = getNodeAssetSubdirectory(meta["PathVars"]);
        string? relatedFieldName = getString(meta["Transfer"]);
        BlueprintVariableEditorKind editorKind = getNodeEditorKind(
            meta,
            assetSubdirectory,
            relatedFieldName);
        JsonNode? value = port.Value?.DeepClone();
        bool constructedType = isConstructedNodeType(type);
        string editorType = constructedType ? "any[]" : type.ToString();
        return new BlueprintVariableField(port.Name, editorType, value)
        {
            Module = namedReference?.ModuleName,
            TypeName = constructedType ? "any[]" : namedReference?.TypeName ?? type.ToString(),
            DefaultValue = value?.DeepClone(),
            Meta = meta,
            UseJsonTableEditor = constructedType,
            PreserveNullValue = true,
            EditorKind = editorKind,
            RelatedFieldName = relatedFieldName,
            AssetSubdirectory = assetSubdirectory,
            Options = getNodeParameterOptions(port.Name, meta),
        };
    }

    public string GetNodeParameterDisplayTypeName(BlueprintGraphPort port)
    {
        JsonObject meta = port.Meta;
        string? assetSubdirectory = getNodeAssetSubdirectory(meta["PathVars"]);
        string? relatedFieldName = getString(meta["Transfer"]);
        BlueprintVariableEditorKind editorKind = getNodeEditorKind(
            meta,
            assetSubdirectory,
            relatedFieldName);
        return editorKind switch
        {
            BlueprintVariableEditorKind.MoveRoute => "MoveRoute",
            BlueprintVariableEditorKind.TransferPosition => "TransferPos",
            BlueprintVariableEditorKind.BlueprintClass => "BlueprintClass",
            BlueprintVariableEditorKind.CommonFunction => "CommonFunction",
            _ => port.TypeName,
        };
    }

    private static BlueprintVariableEditorKind getNodeEditorKind(
        JsonObject meta,
        string? assetSubdirectory,
        string? relatedFieldName)
    {
        if (!string.IsNullOrWhiteSpace(relatedFieldName))
            return BlueprintVariableEditorKind.TransferPosition;
        if (getBool(meta["MoveRouteVars"]))
            return BlueprintVariableEditorKind.MoveRoute;
        if (assetSubdirectory is not null)
            return BlueprintVariableEditorKind.Default;
        if (getBool(meta["BlueprintClassVars"]))
            return BlueprintVariableEditorKind.BlueprintClass;
        if (getBool(meta["CommonFunctionVars"]))
            return BlueprintVariableEditorKind.CommonFunction;
        return BlueprintVariableEditorKind.Default;
    }

    private static bool isConstructedNodeType(LuaMetadataType valueType)
    {
        if (valueType.Kind != LuaMetadataTypeKind.Named)
            return false;
        string type = valueType.Name;
        if (type.EndsWith("Vector2f", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector2i", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector2u", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector3f", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector3i", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Vector3u", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Color", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("Colour", StringComparison.OrdinalIgnoreCase)
            || type.EndsWith("IntRect", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }
        string normalized = type.ToLowerInvariant();
        return normalized is not "pair"
            and not "bool"
            and not "int"
            and not "float"
            and not "double"
            and not "number"
            and not "string"
            and not "any"
            and not "nil"
            and not "table"
            and not "list"
            and not "array"
            and not "dict"
            and not "function"
            and not "event";
    }

    private BlueprintVariableField createFormField(
        ResolvedBlueprintField field,
        JsonNode? defaultValue,
        bool hasDefault,
        string? rectSource,
        HashSet<string> resolving,
        bool isReadOnly = false,
        JsonNode? readOnlyDisplayValue = null,
        GeneralDataFieldSource? structuredGeneralData = null)
    {
        BlueprintFieldMetadata? fieldMetadata = field.Metadata;
        string? defaultModule = fieldMetadata?.DeclaringType.ModuleName;
        IReadOnlyList<BlueprintVariableField> nestedFields = createStructuredFields(
            field.Type,
            defaultModule,
            field.Value,
            resolving,
            structuredGeneralData);
        LuaTypeReference displayType = field.Type.WithDefaultModule(defaultModule);
        JsonObject meta = fieldMetadata?.Meta.DeepClone() as JsonObject ?? [];
        return new BlueprintVariableField(field.Name, displayType.QualifiedName, field.Value)
        {
            Module = displayType.ModuleName,
            TypeName = displayType.TypeName,
            DefaultValue = hasDefault ? cloneNode(defaultValue) : null,
            DisplayValue = isReadOnly && readOnlyDisplayValue is not null
                ? cloneNode(readOnlyDisplayValue)
                : getConfigDisplayValue(field.Name, field.Value, meta),
            Meta = meta,
            IsComponent = fieldMetadata?.Component == true,
            IsReadOnly = isReadOnly,
            PreserveNullValue = field.Value is null,
            RectSourceField = rectSource,
            Options = getGeneralDataOptions(meta),
            Fields = nestedFields,
        };
    }

    private IReadOnlyList<BlueprintVariableField> createStructuredFields(
        LuaTypeReference fieldType,
        string? defaultModule,
        JsonNode? value,
        HashSet<string> resolving,
        GeneralDataFieldSource? generalDataFields)
    {
        LuaTypeReference type = fieldType.WithDefaultModule(defaultModule);
        if (metadataService.GetType(type) is null || !resolving.Add(type.QualifiedName))
            return [];

        IReadOnlyList<LuaTypeMetadata> mro = metadataService.ResolveMro(type);
        List<string> order = [];
        Dictionary<string, BlueprintFieldMetadata> schema = new(StringComparer.Ordinal);
        HashSet<string> invalidVars = new(StringComparer.Ordinal);
        Dictionary<string, string> rectSources = new(StringComparer.Ordinal);
        foreach (LuaTypeMetadata metadata in mro.Reverse())
        {
            foreach (string invalidVar in metadata.InvalidVars)
                invalidVars.Add(invalidVar);
            foreach (KeyValuePair<string, JsonNode?> pair in metadata.RectRangeVars)
            {
                string? source = getString(pair.Value);
                if (source is not null)
                    rectSources[pair.Key] = source;
            }
            foreach (string name in metadata.Attrs)
            {
                if (!metadata.Fields.TryGetValue(name, out BlueprintFieldMetadata? nestedMetadata))
                    continue;
                if (!schema.ContainsKey(name))
                    order.Add(name);
                schema[name] = nestedMetadata;
            }
        }

        JsonObject valueObject = value as JsonObject ?? [];
        List<BlueprintVariableField> result = [];
        HashSet<string> added = new(StringComparer.Ordinal);
        foreach (string name in order)
        {
            if (invalidVars.Contains(name))
                continue;
            BlueprintFieldMetadata metadata = schema[name];
            bool hasValue = valueObject.TryGetPropertyValue(name, out JsonNode? childValue);
            bool hasDefaultValue = metadata.HasDefaultValue;
            JsonNode? childDefault = metadata.DefaultValue;
            if (!hasValue && !hasDefaultValue)
                continue;
            if (!hasValue)
                childValue = childDefault;
            ResolvedBlueprintField nestedField = new(
                name,
                metadata.Type,
                childValue,
                childDefault,
                metadata,
                false,
                hasDefaultValue);
            rectSources.TryGetValue(name, out string? rectSource);
            bool isReadOnly = generalDataFields?.Names.Contains(name) == true;
            JsonNode? displayValue = isReadOnly
                ? getGeneralDataValue(generalDataFields, name)
                : null;
            result.Add(createFormField(
                nestedField,
                childDefault,
                hasDefaultValue,
                rectSource,
                resolving,
                isReadOnly,
                displayValue));
            added.Add(name);
        }

        foreach (KeyValuePair<string, JsonNode?> pair in valueObject)
        {
            if (added.Contains(pair.Key) || invalidVars.Contains(pair.Key))
                continue;
            ResolvedBlueprintField nestedField = new(
                pair.Key,
                inferType(pair.Value),
                pair.Value,
                null,
                null,
                true,
                false);
            result.Add(createFormField(
                nestedField,
                null,
                false,
                null,
                resolving));
        }
        resolving.Remove(type.QualifiedName);
        return result;
    }

    private GeneralDataFieldSource? getGeneralDataFields(ResolvedBlueprintClass resolved)
    {
        ResolvedBlueprintField? idField = resolved.GetField("ID");
        string? dataType = getGeneralDataType(idField?.Metadata?.Meta["GeneralDataVars"]);
        if (dataType is null
            || !gameData.GeneralData.TryGetValue(dataType, out JsonObject? data)
            || data["params"] is not JsonObject parameters)
        {
            return null;
        }
        JsonObject previewValues = [];
        string? memberId = getString(idField?.Value);
        JsonObject? member = memberId is not null
            ? data["members"]?[memberId] as JsonObject
            : null;
        foreach (KeyValuePair<string, JsonNode?> parameter in parameters)
        {
            if (member?.TryGetPropertyValue(parameter.Key, out JsonNode? memberValue) == true)
            {
                previewValues[parameter.Key] = cloneNode(memberValue);
            }
            else if (parameter.Value is JsonObject definition
                && definition.TryGetPropertyValue("defaultValue", out JsonNode? defaultValue))
            {
                previewValues[parameter.Key] = cloneNode(defaultValue);
            }
        }
        return new GeneralDataFieldSource(
            new HashSet<string>(parameters.Select(entry => entry.Key), StringComparer.Ordinal),
            previewValues);
    }

    private static JsonNode? getGeneralDataValue(
        GeneralDataFieldSource? fields,
        string name)
    {
        return fields?.Values.TryGetPropertyValue(name, out JsonNode? value) == true
            ? value
            : null;
    }

    private IReadOnlyList<BlueprintVariableOption> getGeneralDataOptions(JsonObject meta)
    {
        string? dataType = getGeneralDataType(meta["GeneralDataVars"]);
        if (dataType is null)
            return [];
        List<BlueprintVariableOption> options =
        [
            new BlueprintVariableOption(
                LocaleService.Get("GENERAL_DATA_PLACEHOLDER"),
                JsonValue.Create(string.Empty)),
        ];
        IEnumerable<string> keys;
        if (string.Equals(dataType, "ANIMATION", StringComparison.OrdinalIgnoreCase))
        {
            keys = gameData.AnimationsData.Keys;
        }
        else if (gameData.GeneralData.TryGetValue(dataType, out JsonObject? data)
            && data["members"] is JsonObject members)
        {
            keys = members.Select(pair => pair.Key);
        }
        else
        {
            keys = [];
        }
        foreach (string key in keys)
            options.Add(new BlueprintVariableOption(key, JsonValue.Create(key)));
        return options;
    }

    private IReadOnlyList<BlueprintVariableOption> getNodeParameterOptions(
        string parameterName,
        JsonObject meta)
    {
        JsonNode? dropBox = meta["DropBox"];
        if (dropBox is JsonObject map)
            dropBox = map[parameterName];
        if (dropBox is JsonArray values)
        {
            List<BlueprintVariableOption> options = [];
            foreach (JsonNode? value in values)
            {
                string label = getString(value) ?? value?.ToJsonString() ?? string.Empty;
                options.Add(new BlueprintVariableOption(label, value));
            }
            return options;
        }
        return getGeneralDataOptions(meta);
    }

    private static string? getNodeAssetSubdirectory(JsonNode? value)
    {
        if (getString(value) is string path)
            return path;
        return value is JsonValue scalar
            && scalar.TryGetValue(out bool enabled)
            && enabled
            ? string.Empty
            : null;
    }

    private JsonNode? getConfigDisplayValue(string fieldName, JsonNode? value, JsonObject meta)
    {
        if (getString(value) is not string text || text.Length != 0)
            return cloneNode(value);
        (string Config, string Setting)? reference = getConfigReference(meta["ConfigVars"], fieldName);
        if (reference is not { } configReference
            || !gameData.SystemConfigData.TryGetValue(configReference.Config, out JsonObject? config)
            || config[configReference.Setting] is not JsonObject setting
            || !setting.TryGetPropertyValue("value", out JsonNode? configValue))
        {
            return cloneNode(value);
        }
        return cloneNode(configValue);
    }

    private static LuaTypeReference inferType(JsonNode? value)
    {
        if (value is JsonObject)
            return new LuaTypeReference(null, "table");
        if (value is JsonArray)
            return new LuaTypeReference(null, "any[]");
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out bool _))
                return new LuaTypeReference(null, "bool");
            if (scalar.TryGetValue(out string? _))
                return new LuaTypeReference(null, "string");
            if (scalar.TryGetValue(out int _) || scalar.TryGetValue(out long _))
                return new LuaTypeReference(null, "int");
            if (scalar.TryGetValue(out double _) || scalar.TryGetValue(out decimal _))
                return new LuaTypeReference(null, "float");
        }
        return new LuaTypeReference(null, "any");
    }

    private static string? getGeneralDataType(JsonNode? value)
    {
        if (getString(value) is string direct)
            return direct;
        if (value is JsonArray array)
        {
            if (array.Count == 1)
                return getString(array[0]);
            if (array.Count >= 2)
                return getString(array[1]) ?? getString(array[0]);
        }
        if (value is JsonObject data)
            return getString(data["type"] ?? data["dataType"] ?? data["key"]);
        return null;
    }

    private static (string Config, string Setting)? getConfigReference(JsonNode? value, string fieldName)
    {
        if (getString(value) is string direct)
        {
            int separator = direct.IndexOf('.');
            return separator > 0 && separator < direct.Length - 1
                ? (direct[..separator], direct[(separator + 1)..])
                : (direct, fieldName);
        }
        if (value is JsonArray array && array.Count >= 2
            && getString(array[^2]) is string config
            && getString(array[^1]) is string setting)
        {
            return (config, setting);
        }
        if (value is JsonObject reference
            && getString(reference["config"] ?? reference["file"]) is string configName
            && getString(reference["setting"] ?? reference["key"] ?? reference["name"]) is string settingName)
        {
            return (configName, settingName);
        }
        return null;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool getBool(JsonNode? value)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out bool result)
            && result;
    }

    private static JsonNode? cloneNode(JsonNode? value)
    {
        return value?.DeepClone();
    }

    private sealed record GeneralDataFieldSource(
        IReadOnlySet<string> Names,
        JsonObject Values);
}
