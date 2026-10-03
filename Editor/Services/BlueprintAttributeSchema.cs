using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class BlueprintAttributeSchema(LuaMetadataService metadata)
{
    public static InvalidDataException Error(string blueprint, string message) =>
        new($"Invalid Blueprint '{blueprint}': {message}");

    public static bool IsReservedName(string name) =>
        string.IsNullOrWhiteSpace(name) || char.IsDigit(name[0]) || name != name.Trim()
        || name.StartsWith("__", StringComparison.Ordinal)
        || name is "init" or "new" or "scriptMixin" or "scriptPath"
            or "_GENERATED_CLASS" or "_graph" or "_hasImplementationOwner";

    public BlueprintFieldMetadata ReadDefinition(string blueprint, string name, JsonNode? raw)
    {
        if (IsReservedName(name))
            throw Error(blueprint, $"attrDefs.{name} uses an invalid or reserved name");
        if (raw is not JsonObject definition || definition["type"] is not JsonNode typeNode)
            throw Error(blueprint, $"attrDefs.{name} requires an object with a type");
        if (definition.Any(entry => entry.Key is not "type" and not "base"))
            throw Error(blueprint, $"attrDefs.{name} accepts only type and optional file base");
        LuaMetadataType type;
        try
        {
            type = LuaMetadataType.Parse(typeNode);
        }
        catch (InvalidDataException exception)
        {
            throw Error(blueprint, $"attrDefs.{name}.type: {exception.Message}");
        }
        ValidateType(type, blueprint, "attrDefs." + name + ".type");
        JsonObject meta = [];
        if (type.Kind == LuaMetadataTypeKind.Named && type.Name == "file")
        {
            string? baseHint = null;
            if (definition.ContainsKey("base")
                && (definition["base"] is not JsonValue scalar || !scalar.TryGetValue(out baseHint)
                    || !GameAssetPath.IsValidBaseHint(baseHint)))
                throw Error(blueprint, $"attrDefs.{name}.base is invalid");
            meta["PathVars"] = string.IsNullOrEmpty(baseHint) ? GameAssetPath.Root
                : baseHint.StartsWith('/') ? baseHint : GameAssetPath.Root + "/" + baseHint;
        }
        else if (definition.ContainsKey("base"))
            throw Error(blueprint, $"attrDefs.{name}.base requires type file");
        return new BlueprintFieldMetadata(name, LuaTypeReference.FromSchema(type), false, null,
            false, meta, LuaTypeReference.Parse(blueprint));
    }

    public void ValidateType(LuaMetadataType type, string blueprint, string path)
    {
        if (type.Kind == LuaMetadataTypeKind.Enum)
        {
            LuaEnumDefinition definition = metadata.Enums.Read(type.Name);
            if (definition.ValueType is null)
                throw Error(blueprint, path + ": " + definition.Error);
        }
        else if (type.Kind == LuaMetadataTypeKind.Named
            && type.Name is not ("any" or "nil" or "bool" or "int" or "float" or "double" or "number"
                or "string" or "file" or "function" or "event" or "Pair"
                or "sf.Vector2f" or "sf.Vector2i" or "sf.Vector2u" or "sf.Vector3f" or "sf.Vector3i" or "sf.Vector3u"
                or "sf.Color" or "sf.IntRect" or "sf.FloatRect")
            && metadata.GetType(type.Name) is null)
            throw Error(blueprint, path + ": unknown type '" + type.Name + "'");
        foreach (LuaMetadataType argument in type.Arguments)
            ValidateType(argument, blueprint, path);
    }

    public void ValidateValue(LuaMetadataType type, JsonNode? value, string blueprint, string path)
    {
        List<string> errors = [];
        LuaMetadataLiteralValidation.ValidateLiteral(type, value, path, errors, metadata.Enums.Read);
        if (errors.Count != 0)
            throw Error(blueprint, string.Join("; ", errors));
        if (type.Kind == LuaMetadataTypeKind.List && value is JsonArray list)
        {
            for (int index = 0; index < list.Count; index++)
                ValidateValue(type.Arguments[0], list[index], blueprint, $"{path}[{index}]");
        }
        else if (type.Kind == LuaMetadataTypeKind.Dictionary && value is JsonObject dictionary)
        {
            foreach (KeyValuePair<string, JsonNode?> entry in dictionary)
                ValidateValue(type.Arguments[1], entry.Value, blueprint, path + "." + entry.Key);
        }
        else if (type.Kind == LuaMetadataTypeKind.Tuple && value is JsonArray tuple)
        {
            for (int index = 0; index < tuple.Count; index++)
                ValidateValue(type.Arguments[index], tuple[index], blueprint, $"{path}[{index}]");
        }
        else if (type.Kind == LuaMetadataTypeKind.Union && value is JsonObject union)
        {
            LuaMetadataType branch = type.Arguments.First(argument => JsonNode.DeepEquals(argument.ToSchema(), union["$type"]));
            ValidateValue(branch, union["$value"], blueprint, path + ".$value");
        }
        else if (type.Kind == LuaMetadataTypeKind.Named && type.Name == "file"
            && value is JsonValue scalar && scalar.TryGetValue(out string? file) && !string.IsNullOrEmpty(file)
            && !GameAssetPath.IsCanonical(file))
            throw Error(blueprint, path + " must use a canonical /Game/Assets/ path");
        else if (type.Kind == LuaMetadataTypeKind.Named && value is JsonObject record && metadata.GetType(type.Name) is not null)
        {
            Dictionary<string, BlueprintFieldMetadata> fields = new(StringComparer.Ordinal);
            foreach (LuaTypeMetadata owner in metadata.ResolveMro(type.Name).Reverse())
                foreach (KeyValuePair<string, BlueprintFieldMetadata> field in owner.Fields)
                    fields[field.Key] = field.Value;
            foreach (KeyValuePair<string, JsonNode?> entry in record)
            {
                if (!fields.TryGetValue(entry.Key, out BlueprintFieldMetadata? field))
                    throw Error(blueprint, path + "." + entry.Key + " is not declared");
                ValidateValue(field.Type.WithDefaultModule(field.DeclaringType.ModuleName).Schema,
                    entry.Value, blueprint, path + "." + entry.Key);
            }
        }
    }
}
