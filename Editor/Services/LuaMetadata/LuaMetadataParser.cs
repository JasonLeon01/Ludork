using Ludork.Models;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class LuaMetadataParser
{
    public static IReadOnlyDictionary<string, LuaTypeMetadata> ReadFile(string path, string moduleName)
    {
        Table root = readRoot(path);
        return parseMetadataFile(root, moduleName);
    }

    public static LuaTypeMetadata ReadScriptMixin(string path, string moduleName, string typeName)
    {
        Table root = readRoot(path);
        validateScriptMixinMetadata(root, typeName);
        IReadOnlyDictionary<string, LuaTypeMetadata> types = parseMetadataFile(root, moduleName);
        if (types.Count != 1 || !types.TryGetValue(typeName, out LuaTypeMetadata? metadata))
            throw new InvalidDataException($"Mixin metadata must contain only the {typeName} type");
        if (metadata.Bases.Count != 0)
            throw new InvalidDataException("Mixin metadata cannot declare bases");
        return metadata;
    }

    private static Table readRoot(string path)
    {
        Script script = new(CoreModules.None);
        DynValue result = script.DoString(File.ReadAllText(path), null, path);
        validateMetadataRoot(result);
        return result.Table;
    }

    private static void validateMetadataRoot(DynValue value)
    {
        if (value.Type != DataType.Table)
            throw new InvalidDataException();
        HashSet<Table> visiting = new(ReferenceEqualityComparer.Instance);
        validatePureData(value, visiting);
    }

    private static void validateScriptMixinMetadata(Table root, string expectedTypeName)
    {
        List<TablePair> rootPairs = root.Pairs.ToList();
        if (rootPairs.Count != 1
            || rootPairs[0].Key.Type != DataType.String
            || rootPairs[0].Key.String != expectedTypeName
            || rootPairs[0].Value.Type != DataType.Table)
            throw new InvalidDataException($"Mixin metadata must contain only the {expectedTypeName} type");

        Table typeTable = rootPairs[0].Value.Table;
        DynValue attrsValue = typeTable.Get("attrs");
        if (attrsValue.Type != DataType.Table)
            throw new InvalidDataException("Mixin metadata type must declare attrs");
        List<TablePair> attrPairs = attrsValue.Table.Pairs
            .OrderBy(pair => pair.Key.Type == DataType.Number ? pair.Key.Number : double.MaxValue)
            .ToList();
        List<string> attrs = [];
        for (int index = 0; index < attrPairs.Count; index++)
        {
            TablePair pair = attrPairs[index];
            if (pair.Key.Type != DataType.Number
                || pair.Key.Number != index + 1
                || pair.Value.Type != DataType.String
                || string.IsNullOrWhiteSpace(pair.Value.String))
            {
                throw new InvalidDataException("Mixin metadata attrs must be an ordered string array");
            }
            attrs.Add(pair.Value.String);
        }
        if (attrs.Distinct(StringComparer.Ordinal).Count() != attrs.Count)
            throw new InvalidDataException("Mixin metadata attrs must contain unique field names");
        foreach (string attr in attrs)
        {
            DynValue fieldValue = typeTable.Get(attr);
            if (fieldValue.Type != DataType.Table || readFieldType(fieldValue.Table.Get("type")) is null)
                throw new InvalidDataException($"Mixin metadata field {attr} must declare type");
        }

        DynValue basesValue = typeTable.Get("bases");
        if (basesValue.Type == DataType.Table && basesValue.Table.Pairs.Any())
            throw new InvalidDataException("Mixin metadata cannot declare bases");
        if (basesValue.Type is not DataType.Nil and not DataType.Void and not DataType.Table)
            throw new InvalidDataException("Mixin metadata bases must be empty");
    }

    private static void validatePureData(DynValue value, HashSet<Table> visiting)
    {
        if (value.Type == DataType.Number)
        {
            if (!double.IsFinite(value.Number))
                throw new InvalidDataException();
            return;
        }
        if (value.Type is DataType.Nil or DataType.Void or DataType.Boolean or DataType.String)
            return;
        if (value.Type != DataType.Table || value.Table is null || !visiting.Add(value.Table))
            throw new InvalidDataException();

        foreach (TablePair pair in value.Table.Pairs)
        {
            if (pair.Key.Type is not DataType.String and not DataType.Number)
                throw new InvalidDataException();
            if (pair.Key.Type == DataType.Number && !double.IsFinite(pair.Key.Number))
                throw new InvalidDataException();
            validatePureData(pair.Value, visiting);
        }
        visiting.Remove(value.Table);
    }

    private static IReadOnlyDictionary<string, LuaTypeMetadata> parseMetadataFile(Table root, string moduleName)
    {
        Dictionary<string, LuaTypeMetadata> types = new(StringComparer.Ordinal);
        foreach (TablePair pair in root.Pairs)
        {
            if (pair.Key.Type != DataType.String || pair.Value.Type != DataType.Table)
                continue;
            string typeName = pair.Key.String;
            LuaTypeMetadata? metadata = parseType(moduleName, typeName, pair.Value.Table);
            if (metadata is not null)
                types[typeName] = metadata;
        }
        if (types.Count > 1 && types.Values.Any(type => type.ModuleReturn))
            throw new InvalidDataException("Metadata for a directly returned class must contain one type");
        return types;
    }

    private static LuaTypeMetadata? parseType(string moduleName, string typeName, Table table)
    {
        DynValue moduleReturn = table.Get("moduleReturn");
        if (moduleReturn.Type is not DataType.Nil and not DataType.Void and not DataType.Boolean)
            throw new InvalidDataException("moduleReturn must be a boolean");
        LuaTypeReference declaringType = new(moduleName, typeName);
        IReadOnlyList<string> attrs = readStringArray(table.Get("attrs"));
        IReadOnlyList<LuaTypeReference> bases = readBases(table.Get("bases"), moduleName);
        Dictionary<string, BlueprintFieldMetadata> fields = new(StringComparer.Ordinal);
        foreach (string attr in attrs)
        {
            DynValue fieldValue = table.Get(attr);
            if (fieldValue.Type != DataType.Table)
                continue;
            Table fieldTable = fieldValue.Table;
            LuaTypeReference? fieldType = readFieldType(fieldTable.Get("type"));
            if (fieldType is null)
                continue;
            DynValue defaultValue = fieldTable.Get("default");
            bool hasDefaultValue = defaultValue.Type is not DataType.Nil and not DataType.Void;
            JsonNode? defaultNode = hasDefaultValue
                ? toJsonNode(defaultValue, fieldType.Schema)
                : null;
            bool component = fieldTable.Get("component").CastToBool();
            JsonObject meta = toJsonObject(fieldTable.Get("Meta"));
            fields[attr] = new BlueprintFieldMetadata(
                attr,
                fieldType,
                hasDefaultValue,
                defaultNode,
                component,
                meta,
                declaringType
            );
        }

        List<string> memberNames = [];
        Dictionary<string, LuaNodeMemberMetadata> members = new(StringComparer.Ordinal);
        foreach (TablePair pair in table.Pairs)
        {
            if (pair.Key.Type != DataType.String || pair.Value.Type != DataType.Table)
                continue;
            LuaNodeMemberMetadata? member = parseNodeMember(
                pair.Key.String,
                pair.Value.Table,
                declaringType,
                moduleReturn.Type == DataType.Boolean && moduleReturn.Boolean);
            if (member is null)
                continue;
            memberNames.Add(member.Name);
            members[member.Name] = member;
        }

        return new LuaTypeMetadata(
            declaringType,
            attrs,
            bases,
            fields,
            toJsonObject(table.Get("Meta")),
            readStringArray(table.Get("InvalidVars")),
            toJsonObject(table.Get("RectRangeVars")),
            memberNames,
            members,
            moduleReturn.Type == DataType.Boolean && moduleReturn.Boolean
        );
    }

    private static LuaNodeMemberMetadata? parseNodeMember(
        string name,
        Table table,
        LuaTypeReference declaringType,
        bool moduleReturn
    )
    {
        DynValue memberTypeValue = table.Get("type");
        if (memberTypeValue.Type != DataType.String)
            return null;
        LuaNodeMemberKind kind;
        if (memberTypeValue.String == "function")
            kind = LuaNodeMemberKind.Function;
        else if (memberTypeValue.String == "event")
            kind = LuaNodeMemberKind.Event;
        else
            return null;

        List<LuaNodeParameterMetadata> parameters = [];
        DynValue parametersValue = table.Get("parameters");
        IReadOnlyList<string> parameterNames = readStringArray(parametersValue);
        DynValue defaultsValue = table.Get("default");
        IReadOnlyList<string> unsetDefaults = readStringArray(table.Get("defaultUnset"));
        for (int index = 0; index < parameterNames.Count; index++)
        {
            string parameterName = parameterNames[index];
            LuaTypeReference parameterType = parametersValue.Type == DataType.Table
                ? readFieldType(parametersValue.Table.Get(parameterName))
                    ?? new LuaTypeReference(null, "any")
                : new LuaTypeReference(null, "any");
            DynValue defaultValue = defaultsValue.Type == DataType.Table
                ? defaultsValue.Table.Get(index + 1)
                : DynValue.Nil;
            bool hasLiteralDefault = defaultValue.Type is not DataType.Nil and not DataType.Void;
            bool hasDefaultValue = hasLiteralDefault || unsetDefaults.Contains(parameterName, StringComparer.Ordinal);
            parameters.Add(new LuaNodeParameterMetadata(
                parameterName,
                parameterType,
                hasDefaultValue,
                hasLiteralDefault
                    ? toJsonNode(
                        defaultValue,
                        parameterType.Schema)
                    : null
            ));
        }

        IReadOnlyList<LuaNodeReturnMetadata> returns = readReturns(table.Get("return"));
        DynValue execSplitValue = table.Get("ExecSplit");
        DynValue latentValue = table.Get("Latent");
        DynValue latentStatesValue = table.Get("LatentStates");
        DynValue loopNodeValue = table.Get("LoopNode");
        IReadOnlyList<string> latentOutputs = readStringArray(latentValue);
        if (latentOutputs.Count == 0)
            latentOutputs = readStringArray(latentStatesValue);
        return new LuaNodeMemberMetadata(
            name,
            kind,
            parameters,
            returns,
            readStringArray(execSplitValue),
            toJsonObject(execSplitValue),
            latentValue.Type is DataType.Nil or DataType.Void ? null : toJsonNode(latentValue),
            latentOutputs,
            toJsonObject(latentStatesValue),
            loopNodeValue.Type is not DataType.Nil and not DataType.Void
                ? toJsonNode(loopNodeValue)
                : null,
            table.Get("Pure").CastToBool(),
            toJsonObject(table.Get("Meta")),
            declaringType,
            moduleReturn
        );
    }

    private static IReadOnlyList<LuaNodeReturnMetadata> readReturns(DynValue value)
    {
        if (value.Type != DataType.Table)
            return Array.Empty<LuaNodeReturnMetadata>();
        List<LuaNodeReturnMetadata> returns = [];
        HashSet<string> names = new(StringComparer.Ordinal);
        foreach (string name in readStringArray(value))
        {
            if (!names.Add(name))
                continue;
            DynValue declared = value.Table.Get(name);
            LuaTypeReference type = readFieldType(declared)
                ?? new LuaTypeReference(null, declared.Type is DataType.Nil or DataType.Void ? "nil" : "any");
            returns.Add(new LuaNodeReturnMetadata(name, type));
        }
        return returns;
    }

    private static LuaTypeReference? readFieldType(DynValue value)
    {
        if (value.Type == DataType.String && !string.IsNullOrWhiteSpace(value.String))
            return LuaTypeReference.FromSchema(LuaMetadataType.Parse(value.String));
        return value.Type == DataType.Table
            ? LuaTypeReference.FromSchema(LuaMetadataType.Parse(toJsonNode(value)))
            : null;
    }

    private static IReadOnlyList<LuaTypeReference> readBases(DynValue value, string moduleName)
    {
        if (value.Type != DataType.Table)
            return Array.Empty<LuaTypeReference>();
        List<DynValue> entries = readArray(value.Table);
        if (entries.Count == 2 && entries.All(entry => entry.Type == DataType.String))
        {
            LuaTypeReference? directReference = readExplicitTypeReference(value.Table);
            return directReference is null ? Array.Empty<LuaTypeReference>() : [directReference];
        }

        List<LuaTypeReference> bases = [];
        foreach (DynValue entry in entries)
        {
            LuaTypeReference? type = entry.Type switch
            {
                DataType.Table => readExplicitTypeReference(entry.Table),
                DataType.String => readCompatibleBaseReference(entry.String, moduleName),
                _ => null,
            };
            if (type is not null)
                bases.Add(type);
        }
        return bases;
    }

    private static LuaTypeReference? readExplicitTypeReference(Table table)
    {
        DynValue module = table.Get(1);
        DynValue type = table.Get(2);
        return module.Type == DataType.String && type.Type == DataType.String
            && !string.IsNullOrWhiteSpace(module.String) && !string.IsNullOrWhiteSpace(type.String)
            ? new LuaTypeReference(module.String, type.String)
            : null;
    }

    private static LuaTypeReference? readCompatibleBaseReference(string value, string moduleName)
    {
        if (string.IsNullOrWhiteSpace(value))
            return null;
        LuaTypeReference reference = LuaTypeReference.Parse(value);
        return reference.WithDefaultModule(moduleName);
    }

    private static IReadOnlyList<string> readStringArray(DynValue value)
    {
        if (value.Type != DataType.Table)
            return Array.Empty<string>();
        List<string> result = [];
        foreach (DynValue item in readArray(value.Table))
        {
            if (item.Type == DataType.String && !string.IsNullOrWhiteSpace(item.String))
                result.Add(item.String);
        }
        return result;
    }

    private static List<DynValue> readArray(Table table)
    {
        return table.Pairs
            .Where(pair => pair.Key.Type == DataType.Number && pair.Key.Number >= 1 && pair.Key.Number == Math.Truncate(pair.Key.Number))
            .OrderBy(pair => pair.Key.Number)
            .Select(pair => pair.Value)
            .ToList();
    }

    private static JsonObject toJsonObject(DynValue value)
    {
        return toJsonNode(value) is JsonObject result ? result : new JsonObject();
    }

    private static JsonNode? toJsonNode(
        DynValue value,
        LuaMetadataType? declaredType = null)
    {
        if (declaredType?.Kind == LuaMetadataTypeKind.Union)
        {
            if (value.Type != DataType.Table)
                throw new InvalidDataException("Union metadata defaults require $type and $value.");
            JsonNode? branchSchema = toJsonNode(value.Table.Get("$type"));
            LuaMetadataType selected = LuaMetadataType.Parse(branchSchema);
            if (!declaredType.Arguments.Any(branch => JsonNode.DeepEquals(branch.ToSchema(), selected.ToSchema())))
                throw new InvalidDataException("Union metadata default selects an undeclared branch.");
            return new JsonObject
            {
                ["$type"] = selected.ToSchema(),
                ["$value"] = toJsonNode(value.Table.Get("$value"), selected),
            };
        }
        return value.Type switch
        {
            DataType.Nil or DataType.Void => null,
            DataType.Boolean => JsonValue.Create(value.Boolean),
            DataType.Number => numberToJson(value.Number),
            DataType.String => JsonValue.Create(value.String),
            DataType.Table => tableToJson(value.Table, declaredType),
            _ => throw new InvalidDataException(),
        };
    }

    private static JsonNode numberToJson(double value)
    {
        if (value == Math.Truncate(value) && value is >= long.MinValue and <= long.MaxValue)
            return JsonValue.Create((long)value);
        return JsonValue.Create(value);
    }

    private static JsonNode tableToJson(
        Table table,
        LuaMetadataType? declaredType = null)
    {
        List<TablePair> pairs = table.Pairs.ToList();
        bool isArray = pairs.Count != 0 && pairs.All(pair =>
            pair.Key.Type == DataType.Number
            && pair.Key.Number >= 1
            && pair.Key.Number <= pairs.Count
            && pair.Key.Number == Math.Truncate(pair.Key.Number)
        );
        if (declaredType?.Kind == LuaMetadataTypeKind.Dictionary)
            return tableToJsonObject(
                pairs.Where(pair => pair.Key.Type == DataType.String),
                declaredType.Arguments[1]);
        if (declaredType?.Kind == LuaMetadataTypeKind.List)
            return tableToJsonArray(getDeclaredArrayPairs(pairs), declaredType.Arguments[0]);
        if (declaredType?.Kind == LuaMetadataTypeKind.Tuple)
            return tableToJsonTuple(pairs, declaredType.Arguments);
        if (declaredType?.Kind == LuaMetadataTypeKind.Table && pairs.Count == 0)
            return new JsonObject();
        if (isArray)
            return tableToJsonArray(pairs, null);
        return tableToJsonObject(pairs, null);
    }

    private static JsonArray tableToJsonArray(
        IEnumerable<TablePair> pairs,
        LuaMetadataType? itemType)
    {
        JsonArray array = [];
        foreach (TablePair pair in pairs.OrderBy(pair => pair.Key.Number))
            array.Add(toJsonNode(pair.Value, itemType));
        return array;
    }

    private static JsonArray tableToJsonTuple(
        IEnumerable<TablePair> pairs,
        IReadOnlyList<LuaMetadataType> itemTypes)
    {
        Dictionary<int, DynValue> values = pairs
            .Where(pair => pair.Key.Type == DataType.Number
                && pair.Key.Number >= 1
                && pair.Key.Number == Math.Truncate(pair.Key.Number)
                && pair.Key.Number <= int.MaxValue)
            .ToDictionary(
                pair => checked((int)pair.Key.Number),
                pair => pair.Value);
        JsonArray array = [];
        for (int index = 0; index < itemTypes.Count; index++)
        {
            LuaMetadataType itemType = itemTypes[index];
            array.Add(values.TryGetValue(index + 1, out DynValue? value)
                ? toJsonNode(value, itemType)
                : LuaMetadataValueDefaults.Create(
                    itemType,
                    _ => JsonValue.Create(string.Empty)));
        }
        return array;
    }

    private static IEnumerable<TablePair> getDeclaredArrayPairs(IEnumerable<TablePair> pairs)
    {
        return pairs
            .Where(pair => pair.Key.Type == DataType.Number
                && pair.Key.Number >= 1
                && pair.Key.Number == Math.Truncate(pair.Key.Number))
            .OrderBy(pair => pair.Key.Number);
    }

    private static JsonObject tableToJsonObject(
        IEnumerable<TablePair> pairs,
        LuaMetadataType? valueType)
    {
        JsonObject result = [];
        foreach (TablePair pair in pairs)
        {
            string key = pair.Key.Type == DataType.String
                ? pair.Key.String
                : pair.Key.Number.ToString(CultureInfo.InvariantCulture);
            result[key] = toJsonNode(pair.Value, valueType);
        }
        return result;
    }

}
