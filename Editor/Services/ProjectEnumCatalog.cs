using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace Ludork.Services;

public sealed class ProjectEnumCatalog
{
    private static readonly HashSet<string> luaKeywords = new(StringComparer.Ordinal)
    {
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function", "global", "goto", "if",
        "in", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while",
    };

    public IReadOnlyDictionary<string, LuaEnumDefinition> Definitions { get; }
    public IReadOnlyDictionary<string, ProjectEnumReference> References { get; }
    public IReadOnlyDictionary<string, string> GeneralDataModules { get; }

    public ProjectEnumCatalog(IReadOnlyDictionary<string, JsonObject> generalData,
        IEnumerable<string> animationKeys, IEnumerable<string> particleKeys)
    {
        Dictionary<string, LuaEnumDefinition> definitions = new(StringComparer.Ordinal);
        Dictionary<string, ProjectEnumReference> references = new(StringComparer.Ordinal);
        Dictionary<string, string> modules = new(StringComparer.Ordinal);
        List<BlueprintVariableOption> typeOptions = [];
        HashSet<string> names = new(StringComparer.OrdinalIgnoreCase) { "GeneralDataKey" };
        foreach (string key in generalData.Keys.Where(key => key.Length != 0).OrderBy(key => key, StringComparer.Ordinal))
        {
            string name = uniqueIdentifier(toTypeIdentifier(key), names);
            string module = "Enums.GeneralData." + name;
            modules[key] = module;
            typeOptions.Add(new BlueprintVariableOption(name, JsonValue.Create(key)));
            add(module, options(generalData[key]["members"] is JsonObject members ? members.Select(pair => pair.Key) : []),
                new ProjectEnumReference("generalMember", key));
        }
        add("Enums.GeneralDataKey", typeOptions, new ProjectEnumReference("generalType", null));
        add("Enums.Animation", options(animationKeys), new ProjectEnumReference("animation", null));
        add("Enums.Particle", options(particleKeys), new ProjectEnumReference("particle", null));
        Definitions = definitions;
        References = references;
        GeneralDataModules = modules;

        void add(string module, IReadOnlyList<BlueprintVariableOption> entries, ProjectEnumReference reference)
        {
            definitions[module] = new LuaEnumDefinition(module, LuaMetadataType.Parse("string"), entries.OrderBy(option => option.Label, StringComparer.Ordinal).ToArray(), null);
            references[module] = reference;
        }
    }

    public string GetGeneralDataModuleName(string typeKey) => GeneralDataModules[typeKey];

    public static bool IsManagedModule(string module) => module is "Enums.GeneralDataKey" or "Enums.Animation" or "Enums.Particle"
        || module.StartsWith("Enums.GeneralData.", StringComparison.Ordinal);

    private static IReadOnlyList<BlueprintVariableOption> options(IEnumerable<string> keys)
    {
        HashSet<string> names = new(StringComparer.Ordinal);
        return keys.Where(key => key.Length != 0).OrderBy(key => key, StringComparer.Ordinal)
            .Select(key => new BlueprintVariableOption(uniqueIdentifier(toMemberIdentifier(key), names), JsonValue.Create(key)))
            .ToArray();
    }

    private static string toTypeIdentifier(string value)
    {
        string[] parts = Regex.Split(value, "[^0-9A-Za-z]+")
            .Where(part => part.Length != 0)
            .ToArray();
        string result = string.Concat(parts.Select(part => char.ToUpperInvariant(part[0]) + part[1..]));
        if (result.Length == 0)
            result = "GeneralData";
        if (char.IsDigit(result[0]))
            result = "GeneralData" + result;
        if (luaKeywords.Contains(result))
            result += "Data";
        return result;
    }

    private static string toMemberIdentifier(string value)
    {
        string result = Regex.Replace(value, "[^0-9A-Za-z_]", "_").Trim('_');
        if (result.Length == 0)
            result = "Key";
        if (char.IsDigit(result[0]))
            result = "_" + result;
        if (luaKeywords.Contains(result))
            result += "_";
        return result;
    }

    private static string uniqueIdentifier(string value, HashSet<string> usedNames)
    {
        string candidate = value;
        int suffix = 2;
        while (!usedNames.Add(candidate))
            candidate = value + "_" + suffix++;
        return candidate;
    }

}
