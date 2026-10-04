using Ludork.Models;
using MoonSharp.Interpreter;
using System;
using System.Collections.Generic;
using System.IO;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace Ludork.Services;

public sealed class LuaEnumService
{
    private static readonly Regex Tokens = new(
        "\\G(?:(?<space>\\s+|--\\[(?<commentEquals>=*)\\[[\\s\\S]*?\\]\\k<commentEquals>\\]|--[^\\r\\n]*)|(?<text>\"(?:\\\\[\\s\\S]|[^\"\\\\])*\"|'(?:\\\\[\\s\\S]|[^'\\\\])*'|\\[(?<stringEquals>=*)\\[[\\s\\S]*?\\]\\k<stringEquals>\\])|(?<number>(?:0[xX][0-9a-fA-F]+(?:\\.[0-9a-fA-F]*)?(?:[pP][+-]?[0-9]+)?|(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?))|(?<name>[a-zA-Z_][a-zA-Z_0-9]*)|(?<symbol>[{}()\\[\\]=,;+-]))",
        RegexOptions.CultureInvariant,
        TimeSpan.FromSeconds(1));
    private readonly string projectPath;

    private readonly Func<ProjectEnumCatalog>? catalog;

    public LuaEnumService(string projectPath, Func<ProjectEnumCatalog>? catalog = null)
    {
        this.projectPath = projectPath;
        this.catalog = catalog;
    }

    public IReadOnlyList<string> EnumerateModules()
    {
        string root = Path.Combine(projectPath, "Scripts", "Enums");
        return (Directory.Exists(root) ? Directory.EnumerateFiles(root, "*.lua", SearchOption.AllDirectories) : [])
            .Select(path => Path.GetRelativePath(root, path)[..^4])
            .Where(path => path.Split(Path.DirectorySeparatorChar).All(part => part.Length > 0
                && (char.IsAsciiLetter(part[0]) || part[0] == '_')
                && part.All(character => char.IsAsciiLetterOrDigit(character) || character == '_')))
            .Select(path => "Enums." + path.Replace(Path.DirectorySeparatorChar, '.'))
            .Where(module => catalog is null || !ProjectEnumCatalog.IsManagedModule(module))
            .Concat(catalog?.Invoke().Definitions.Keys ?? [])
            .Distinct(StringComparer.Ordinal)
            .OrderBy(module => module, StringComparer.Ordinal)
            .ToArray();
    }

    public bool IsProjectModule(string moduleName) => catalog?.Invoke().Definitions.ContainsKey(moduleName) == true;

    public bool TryGetReference(string moduleName, out ProjectEnumReference? reference)
    {
        reference = null;
        return catalog?.Invoke().References.TryGetValue(moduleName, out reference) == true;
    }

    public LuaEnumDefinition Read(string moduleName) => Read(LuaMetadataType.Parse(new JsonObject { ["enum"] = moduleName }));

    public LuaEnumDefinition Read(LuaMetadataType schema)
    {
        string moduleName = schema.Name;
        try
        {
            LuaMetadataType.ValidateEnumModule(moduleName);
            LuaMetadataType? declared = schema.EnumValueType;
            if (catalog is not null && ProjectEnumCatalog.IsManagedModule(moduleName))
            {
                if (!catalog().Definitions.TryGetValue(moduleName, out LuaEnumDefinition? managed))
                    throw new InvalidDataException("Project enum does not exist.");
                if (declared is not null && declared.Name != "string")
                    throw new InvalidDataException("Project enum values require string valueType.");
                return managed;
            }
            string path = Path.Combine(projectPath, "Scripts", moduleName.Replace('.', Path.DirectorySeparatorChar) + ".lua");
            string source = File.ReadAllText(path);
            Script script = new(CoreModules.None);
            script.LoadString(source, null, path);
            IReadOnlyDictionary<string, JsonNode> constants = readConstants(source, script);
            List<BlueprintVariableOption> options = [];
            string? valueKind = null;
            bool integers = true;
            foreach (KeyValuePair<string, JsonNode> pair in constants)
            {
                if (string.IsNullOrWhiteSpace(pair.Key))
                    throw new InvalidDataException("Enum keys must be non-empty strings.");
                JsonValue value = (JsonValue)pair.Value;
                string kind = value.TryGetValue(out string? _) ? "string" : value.TryGetValue(out bool _) ? "bool" : "number";
                if (valueKind is not null && valueKind != kind)
                    throw new InvalidDataException("Enum values must all have the same scalar type.");
                valueKind = kind;
                if (kind == "number")
                    integers &= value.TryGetValue(out long _);
                options.Add(new BlueprintVariableOption(pair.Key, value));
            }
            if (options.Count == 0)
            {
                if (declared is null)
                    throw new InvalidDataException("Empty enum module requires an explicit valueType.");
                return new LuaEnumDefinition(moduleName, declared, [], null);
            }
            string valueType = valueKind == "string" ? "string" : valueKind == "bool" ? "bool" : integers ? "int" : "float";
            if (declared is not null && declared.Name != valueType && !(declared.Name == "float" && valueType == "int"))
                throw new InvalidDataException("Enum constants conflict with the declared valueType.");
            return new LuaEnumDefinition(moduleName, declared ?? LuaMetadataType.Parse(valueType), options.OrderBy(option => option.Label, StringComparer.Ordinal).ToArray(), null);
        }
        catch (Exception exception) when (exception is IOException or InvalidDataException or UnauthorizedAccessException or InterpreterException or ArgumentException or RegexMatchTimeoutException)
        {
            return new LuaEnumDefinition(moduleName, null, [], moduleName + ": " + exception.Message);
        }
    }

    private static IReadOnlyDictionary<string, JsonNode> readConstants(string source, Script script)
    {
        List<(string Kind, string Text)> tokens = [];
        for (int position = 0; position < source.Length;)
        {
            Match match = Tokens.Match(source, position);
            if (!match.Success)
                throw new InvalidDataException("Enum module may contain only a returned table of scalar literals.");
            position += match.Length;
            if (match.Groups["space"].Success)
                continue;
            string kind = match.Groups["text"].Success ? "text" : match.Groups["number"].Success ? "number" : match.Groups["name"].Success ? "name" : "symbol";
            tokens.Add((kind, match.Value));
        }
        Dictionary<string, JsonNode> constants = new(StringComparer.Ordinal);
        int index = 0;
        bool take(string expected)
        {
            if (index >= tokens.Count || tokens[index].Text != expected)
                return false;
            index++;
            return true;
        }
        void require(string expected)
        {
            if (!take(expected))
                throw new InvalidDataException("Enum module may contain only a returned table of scalar literals.");
        }
        string? localName = null;
        if (take("local"))
        {
            if (index >= tokens.Count || tokens[index].Kind != "name")
                throw new InvalidDataException("Enum module must declare one local constant table.");
            localName = tokens[index++].Text;
            require("=");
        }
        else
        {
            require("return");
        }
        require("{");
        while (!take("}"))
        {
            string key;
            if (take("["))
            {
                if (index >= tokens.Count || tokens[index].Kind != "text")
                    throw new InvalidDataException("Enum keys must be string literals or identifiers.");
                key = script.DoString("return " + tokens[index++].Text).String;
                require("]");
            }
            else
            {
                if (index >= tokens.Count || tokens[index].Kind != "name")
                    throw new InvalidDataException("Enum keys must be string literals or identifiers.");
                key = tokens[index++].Text;
            }
            require("=");
            bool parenthesized = take("(");
            bool negative = take("-");
            if (index >= tokens.Count)
                throw new InvalidDataException("Enum value is missing.");
            (string Kind, string Text) value = tokens[index++];
            if (negative && value.Kind != "number"
                || value.Kind is not ("text" or "number") && value.Text is not ("true" or "false"))
            {
                throw new InvalidDataException("Enum values must be string, boolean or finite number literals.");
            }
            JsonNode literal = value.Kind == "text"
                ? JsonValue.Create(script.DoString("return " + value.Text).String)!
                : value.Kind == "number" ? readNumber((negative ? "-" : string.Empty) + value.Text, script)
                : JsonValue.Create(value.Text == "true")!;
            if (parenthesized)
                require(")");
            constants[key] = literal;
            if (!take(",") && !take(";"))
            {
                require("}");
                break;
            }
        }
        take(";");
        if (localName is not null)
        {
            require("return");
            require(localName);
            take(";");
        }
        if (index != tokens.Count)
            throw new InvalidDataException("Enum module must return only its constant table.");
        return constants;
    }

    private static JsonNode readNumber(string text, Script script)
    {
        if (long.TryParse(text, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out long integer))
            return JsonValue.Create(integer)!;
        bool negative = text.StartsWith("-", StringComparison.Ordinal);
        string unsigned = negative ? text[1..] : text;
        if (unsigned.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
            && ulong.TryParse(unsigned[2..], NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture, out ulong hexadecimal))
        {
            long signed = unchecked((long)hexadecimal);
            return JsonValue.Create(negative ? unchecked(-signed) : signed)!;
        }
        double number = script.DoString("return " + text).Number;
        if (!double.IsFinite(number))
            throw new InvalidDataException("Enum numbers must be finite.");
        return JsonValue.Create(number)!;
    }
}
