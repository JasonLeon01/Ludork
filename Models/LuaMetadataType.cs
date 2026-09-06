using System;
using System.Collections.Generic;

namespace Ludork.Models;

internal enum LuaMetadataTypeKind
{
    Named,
    List,
    Dictionary,
    Tuple,
    Table,
}

internal sealed class LuaMetadataType
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

    public override string ToString()
    {
        return Kind switch
        {
            LuaMetadataTypeKind.List => $"{Arguments[0]}[]",
            LuaMetadataTypeKind.Dictionary => $"Dict[{Arguments[0]}, {Arguments[1]}]",
            LuaMetadataTypeKind.Tuple => $"Tuple[{string.Join(", ", Arguments)}]",
            LuaMetadataTypeKind.Table => "table",
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
