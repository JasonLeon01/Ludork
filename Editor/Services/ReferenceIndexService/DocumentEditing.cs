using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    public IReadOnlyList<ReferenceRewrite> PrepareDocumentRename(string section, string oldKey, string newKey)
    {
        if (section == "Maps")
            return PrepareMapReferenceRewrites(new Dictionary<string, string> { [oldKey + ".json"] = newKey + ".json" });
        string oldPath = section == "WorldMaps"
            ? Path.Combine(gameData.ProjectPath, "Data", "Maps", oldKey, "_world.json")
            : dataPath(section, oldKey);
        MarkDirty();
        string? target = GetNodeIdForPath(oldPath);
        if (target is null)
            return [];
        List<ReferenceRewrite> result = [];
        foreach (IGrouping<string, ReferenceRecord> group in GetIncoming(target).GroupBy(record => GetNodePath(record.Source)))
        {
            EditorDocument? document = gameData.GetDocumentByPath(group.Key);
            if (document?.Data is not JsonObject original)
                throw new InvalidDataException($"The referenced document could not be read: {group.Key}");
            JsonObject candidate = (JsonObject)original.DeepClone();
            HashSet<string> paths = group.Select(record => getDocumentReferencePath(document, original, record.Path)).ToHashSet(StringComparer.Ordinal);
            HashSet<string> dictionaryKeys = getDictionaryKeyReferences(document, original);
            HashSet<string> rewritten = new(StringComparer.Ordinal);
            rewriteIndexedValues(candidate, document.Section + "/" + document.Key, paths,
                value => renameReferenceValue(value, section, oldKey, newKey), rewritten, dictionaryKeys);
            if (!paths.SetEquals(rewritten))
                throw new InvalidDataException("The indexed reference could not be updated: "
                    + string.Join(", ", paths.Except(rewritten)));
            if (!JsonNode.DeepEquals(original, candidate))
                result.Add(new ReferenceRewrite(document.Section, document.Key, original, candidate));
        }
        return result;
    }

    private static HashSet<string> getDictionaryKeyReferences(EditorDocument document, JsonObject data)
    {
        HashSet<string> result = new(StringComparer.Ordinal);
        if (document.Section != "General" || data["params"] is not JsonObject schema || data["members"] is not JsonObject members)
            return result;
        foreach (KeyValuePair<string, JsonNode?> parameter in schema)
        {
            if (parameter.Value is not JsonObject definition || getString(definition["type"]) != "dict"
                || definition["reference"] is not JsonObject)
                continue;
            foreach (KeyValuePair<string, JsonNode?> member in members)
            {
                if (member.Value?[parameter.Key] is not JsonObject dictionary)
                    continue;
                foreach (string key in dictionary.Select(pair => pair.Key))
                    result.Add("General/" + document.Key + ".members." + member.Key + "." + parameter.Key + "." + key);
            }
        }
        return result;
    }

    private static string getDocumentReferencePath(EditorDocument document, JsonObject data, string path)
    {
        if (document.Section == "General" && data["members"] is JsonObject members)
        {
            foreach (string member in members.Select(pair => pair.Key))
            {
                string prefix = "General/" + document.Key + "/" + member + ".";
                if (path.StartsWith(prefix, StringComparison.Ordinal))
                    return "General/" + document.Key + ".members." + member + "." + path[prefix.Length..];
            }
        }
        return path;
    }

    private static void rewriteIndexedValues(
        JsonNode node,
        string path,
        IReadOnlySet<string> paths,
        Func<string, string> rewrite,
        ISet<string> rewritten,
        IReadOnlySet<string> dictionaryKeys)
    {
        if (node is JsonObject value)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in value.ToArray())
            {
                string childPath = path + "." + pair.Key;
                if (paths.Contains(childPath) && dictionaryKeys.Contains(childPath))
                {
                    string newKey = rewrite(pair.Key);
                    if (newKey != pair.Key && value.ContainsKey(newKey))
                        throw new InvalidDataException($"The renamed reference conflicts with an existing dictionary key: {childPath}");
                    value.Remove(pair.Key);
                    value[newKey] = pair.Value;
                    rewritten.Add(childPath);
                }
                else if (paths.Contains(childPath) && getString(pair.Value) is string text)
                {
                    value[pair.Key] = rewrite(text);
                    rewritten.Add(childPath);
                }
                else if (pair.Value is not null)
                    rewriteIndexedValues(pair.Value, childPath, paths, rewrite, rewritten, dictionaryKeys);
            }
        }
        else if (node is JsonArray array)
        {
            for (int index = 0; index < array.Count; index++)
            {
                string childPath = path + "[" + index + "]";
                if (paths.Contains(childPath) && getString(array[index]) is string text)
                {
                    array[index] = rewrite(text);
                    rewritten.Add(childPath);
                }
                else if (array[index] is JsonNode child)
                    rewriteIndexedValues(child, childPath, paths, rewrite, rewritten, dictionaryKeys);
            }
        }
    }

    private static string renameReferenceValue(string value, string section, string oldKey, string newKey)
    {
        string text = value.Trim();
        char? quote = text.Length >= 2 && text[0] == text[^1] && text[0] is '\'' or '"' ? text[0] : null;
        string content = quote is not null ? text[1..^1] : text;
        if (section == "UI" && content.StartsWith("Project:", StringComparison.Ordinal))
            content = "Project:" + UiAssetSchema.ToLogicalAssetKey(newKey);
        else if (content.StartsWith("Data." + section + ".", StringComparison.Ordinal))
            content = "Data." + section + "." + newKey.Replace('/', '.');
        else if (content == oldKey)
            content = newKey;
        else if (content == oldKey + ".json")
            content = newKey + ".json";
        else if (content.EndsWith("/" + oldKey + ".json", StringComparison.Ordinal))
            content = content[..^(oldKey.Length + 5)] + newKey + ".json";
        else if (content.EndsWith("/" + oldKey, StringComparison.Ordinal))
            content = content[..^oldKey.Length] + newKey;
        else
            throw new InvalidDataException($"Unsupported indexed reference format: {value}");
        return quote is not null ? quote + content + quote : content;
    }
}
