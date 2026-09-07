using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public enum LuaMetadataTypeKind
{
    Named,
    List,
    Dictionary,
    Tuple,
    Table,
    Union,
}

public sealed class LuaMetadataType
{
    private LuaMetadataType(
        LuaMetadataTypeKind kind,
        string name,
        IReadOnlyList<LuaMetadataType> arguments)
    {
        Kind = kind;
        Name = name;
        Arguments = arguments;
    }

    public LuaMetadataTypeKind Kind { get; }
    public string Name { get; }
    public IReadOnlyList<LuaMetadataType> Arguments { get; }
    public bool IsAny => Kind == LuaMetadataTypeKind.Named
        && string.Equals(Name, "any", StringComparison.OrdinalIgnoreCase);

    public static LuaMetadataType Parse(string typeName)
    {
        string text = string.IsNullOrWhiteSpace(typeName) ? "any" : typeName.Trim();
        int arrayDepth = 0;
        while (text.EndsWith("[]", StringComparison.Ordinal))
        {
            arrayDepth++;
            text = text[..^2].TrimEnd();
        }

        LuaMetadataType result = parsePrimary(text);
        for (int index = 0; index < arrayDepth; index++)
            result = createList(result);
        return result;
    }

    public static LuaMetadataType Parse(JsonNode? schema)
    {
        if (schema is JsonValue scalar && scalar.TryGetValue(out string? name) && !string.IsNullOrWhiteSpace(name))
            return Parse(name);
        if (schema is JsonArray reference && reference.Count == 2
            && reference[0] is JsonValue module && module.TryGetValue(out string? moduleName)
            && reference[1] is JsonValue type && type.TryGetValue(out string? typeName))
        {
            return Parse($"{moduleName}.{typeName}");
        }
        if (schema is not JsonObject map || map.Count != 1)
            throw new InvalidDataException("Metadata type must be a name, module reference or structured schema.");
        if (map.TryGetPropertyValue("list", out JsonNode? list))
            return createList(Parse(list));
        if (map.TryGetPropertyValue("dict", out JsonNode? dictionary))
            return createDictionary(createNamed("string"), Parse(dictionary));
        foreach ((string key, LuaMetadataTypeKind kind) in new[]
        {
            ("union", LuaMetadataTypeKind.Union),
            ("tuple", LuaMetadataTypeKind.Tuple),
        })
        {
            if (!map.TryGetPropertyValue(key, out JsonNode? value))
                continue;
            if (value is not JsonArray entries || entries.Count == 0)
                throw new InvalidDataException($"Metadata {key} requires an ordered non-empty type list.");
            LuaMetadataType[] arguments = entries.Select(Parse).ToArray();
            if (kind == LuaMetadataTypeKind.Union
                && arguments.Select(argument => argument.ToString()).Distinct(StringComparer.Ordinal).Count() != arguments.Length)
            {
                throw new InvalidDataException("Metadata union contains duplicate branches.");
            }
            return new LuaMetadataType(kind, key, arguments);
        }
        throw new InvalidDataException("Unknown metadata type schema.");
    }

    public JsonNode ToSchema()
    {
        return Kind switch
        {
            LuaMetadataTypeKind.List => new JsonObject { ["list"] = Arguments[0].ToSchema() },
            LuaMetadataTypeKind.Dictionary => new JsonObject { ["dict"] = Arguments[1].ToSchema() },
            LuaMetadataTypeKind.Tuple => new JsonObject { ["tuple"] = new JsonArray(Arguments.Select(argument => argument.ToSchema()).ToArray()) },
            LuaMetadataTypeKind.Union => new JsonObject { ["union"] = new JsonArray(Arguments.Select(argument => argument.ToSchema()).ToArray()) },
            _ => JsonValue.Create(Name)!,
        };
    }

    public bool ContainsUnion => Kind == LuaMetadataTypeKind.Union || Arguments.Any(argument => argument.ContainsUnion);

    public bool IsAssignableTo(LuaMetadataType target, Func<string, string, bool>? isDerived = null)
    {
        if (IsAny || target.IsAny)
            return true;
        if (Kind == LuaMetadataTypeKind.Union)
            return Arguments.All(argument => argument.IsAssignableTo(target, isDerived));
        if (target.Kind == LuaMetadataTypeKind.Union)
            return target.Arguments.Any(argument => IsAssignableTo(argument, isDerived));
        if (Kind != target.Kind)
            return false;
        if (Kind != LuaMetadataTypeKind.Named)
            return Arguments.Count == target.Arguments.Count
                && Arguments.Zip(target.Arguments).All(pair => pair.First.IsAssignableTo(pair.Second, isDerived));
        return string.Equals(Name, target.Name, StringComparison.Ordinal)
            || Name == "int" && target.Name == "float"
            || isDerived?.Invoke(Name, target.Name) == true;
    }

    public override string ToString()
    {
        return Kind switch
        {
            LuaMetadataTypeKind.List => $"{Arguments[0]}[]",
            LuaMetadataTypeKind.Dictionary => $"Dict[{Arguments[0]}, {Arguments[1]}]",
            LuaMetadataTypeKind.Tuple => $"Tuple[{string.Join(", ", Arguments)}]",
            LuaMetadataTypeKind.Table => "table",
            LuaMetadataTypeKind.Union => $"Union[{string.Join(", ", Arguments)}]",
            _ => Name,
        };
    }

    private static LuaMetadataType parsePrimary(string text)
    {
        if (string.IsNullOrWhiteSpace(text))
            return createNamed("any");
        if (string.Equals(text, "table", StringComparison.OrdinalIgnoreCase))
            return new LuaMetadataType(LuaMetadataTypeKind.Table, "table", []);
        if (string.Equals(text, "list", StringComparison.OrdinalIgnoreCase)
            || string.Equals(text, "array", StringComparison.OrdinalIgnoreCase))
        {
            return createList(createNamed("any"));
        }
        if (string.Equals(text, "dict", StringComparison.OrdinalIgnoreCase)
            || string.Equals(text, "dictionary", StringComparison.OrdinalIgnoreCase)
            || string.Equals(text, "map", StringComparison.OrdinalIgnoreCase))
        {
            return createDictionary(createNamed("string"), createNamed("any"));
        }

        int open = text.IndexOf('[');
        if (open <= 0 || text[^1] != ']')
            return createNamed(text);
        string containerName = text[..open].Trim();
        string body = text[(open + 1)..^1];
        IReadOnlyList<string>? parts = splitArguments(body);
        if (parts is null)
            return createNamed(text);
        List<LuaMetadataType> arguments = [];
        foreach (string part in parts)
            arguments.Add(Parse(part));

        if ((string.Equals(containerName, "List", StringComparison.OrdinalIgnoreCase)
                || string.Equals(containerName, "Array", StringComparison.OrdinalIgnoreCase))
            && arguments.Count == 1)
        {
            return createList(arguments[0]);
        }
        if ((string.Equals(containerName, "Dict", StringComparison.OrdinalIgnoreCase)
                || string.Equals(containerName, "Dictionary", StringComparison.OrdinalIgnoreCase)
                || string.Equals(containerName, "Map", StringComparison.OrdinalIgnoreCase))
            && arguments.Count == 2
            && arguments[0].Kind == LuaMetadataTypeKind.Named
            && string.Equals(arguments[0].Name, "string", StringComparison.OrdinalIgnoreCase))
        {
            return createDictionary(arguments[0], arguments[1]);
        }
        if (string.Equals(containerName, "Union", StringComparison.OrdinalIgnoreCase)
            && arguments.Count > 0)
        {
            return new LuaMetadataType(LuaMetadataTypeKind.Union, "Union", arguments);
        }
        if (string.Equals(containerName, "Tuple", StringComparison.OrdinalIgnoreCase)
            && arguments.Count > 0)
        {
            return new LuaMetadataType(LuaMetadataTypeKind.Tuple, "Tuple", arguments);
        }
        return createNamed(text);
    }

    private static IReadOnlyList<string>? splitArguments(string body)
    {
        List<string> result = [];
        int depth = 0;
        int start = 0;
        for (int index = 0; index < body.Length; index++)
        {
            char current = body[index];
            if (current == '[')
            {
                depth++;
            }
            else if (current == ']')
            {
                depth--;
                if (depth < 0)
                    return null;
            }
            else if (current == ',' && depth == 0)
            {
                string part = body[start..index].Trim();
                if (part.Length == 0)
                    return null;
                result.Add(part);
                start = index + 1;
            }
        }
        if (depth != 0)
            return null;
        string last = body[start..].Trim();
        if (last.Length == 0)
            return null;
        result.Add(last);
        return result;
    }

    private static LuaMetadataType createNamed(string name)
    {
        string value = string.IsNullOrWhiteSpace(name) ? "any" : name.Trim();
        return new LuaMetadataType(LuaMetadataTypeKind.Named, value, []);
    }

    private static LuaMetadataType createList(LuaMetadataType itemType)
    {
        return new LuaMetadataType(LuaMetadataTypeKind.List, "List", [itemType]);
    }

    private static LuaMetadataType createDictionary(
        LuaMetadataType keyType,
        LuaMetadataType valueType)
    {
        return new LuaMetadataType(
            LuaMetadataTypeKind.Dictionary,
            "Dict",
            [keyType, valueType]);
    }
}
