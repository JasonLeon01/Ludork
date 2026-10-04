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
        string? target = GetNodeIdForPath(oldPath);
        if (target is null)
            return [];
        Dictionary<string, string> enumModuleRenames = new(StringComparer.Ordinal);
        List<ReferenceRecord> incoming = [.. GetIncoming(target)];
        if (section == "General")
        {
            ProjectEnumCatalog before = gameData.GetProjectEnumCatalog();
            Dictionary<string, JsonObject> general = gameData.ReferenceDocuments
                .Where(document => document.Section == "General")
                .ToDictionary(document => document.Key == oldKey ? newKey : document.Key,
                    document => document.Data, StringComparer.Ordinal);
            ProjectEnumCatalog after = new(general, [], []);
            foreach (KeyValuePair<string, string> entry in before.GeneralDataModules)
            {
                string renamedType = entry.Key == oldKey ? newKey : entry.Key;
                string renamedModule = after.GeneralDataModules[renamedType];
                enumModuleRenames[entry.Value] = renamedModule;
                if (entry.Value == renamedModule)
                    continue;
                incoming.AddRange(GetIncoming(ReferenceIdentity.NodeId("general", entry.Key))
                    .Where(record => record.Kind == "generalType"));
            }
        }
        List<ReferenceRewrite> result = [];
        foreach (IGrouping<string, ReferenceRecord> group in incoming.Where(record => record.Kind != "generalTypeDependency").Distinct().GroupBy(record => GetNodePath(record.Source)))
        {
            EditorDocument? document = gameData.GetDocumentByPath(group.Key);
            if (document?.Data is not JsonObject original)
                throw new InvalidDataException($"The referenced document could not be read: {group.Key}");
            JsonObject candidate = (JsonObject)original.DeepClone();
            HashSet<(string Path, bool IsDictionaryKey)> paths = group.Select(record =>
                (getDocumentReferencePath(document, original, record.Path), record.IsDictionaryKey)).ToHashSet();
            HashSet<(string Path, bool IsDictionaryKey)> enumSchemas = group
                .Where(record => record.Kind == "generalType")
                .Select(record => (getDocumentReferencePath(document, original, record.Path), record.IsDictionaryKey)).ToHashSet();
            HashSet<(string Path, bool IsDictionaryKey)> rewritten = [];
            rewriteIndexedValues(candidate, document.Section + "/" + document.Key, paths,
                (path, isDictionaryKey, value) => enumSchemas.Contains((path, isDictionaryKey))
                    && enumModuleRenames.TryGetValue(value, out string? module)
                    ? module : renameReferenceValue(value, section, oldKey, newKey), rewritten);
            if (!paths.SetEquals(rewritten))
                throw new InvalidDataException("The indexed reference could not be updated: "
                    + string.Join(", ", paths.Except(rewritten)));
            if (!JsonNode.DeepEquals(original, candidate))
                result.Add(new ReferenceRewrite(document.Section, document.Key, original, candidate));
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
        IReadOnlySet<(string Path, bool IsDictionaryKey)> paths,
        Func<string, bool, string, string> rewrite,
        ISet<(string Path, bool IsDictionaryKey)> rewritten)
    {
        if (node is JsonObject value)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in value.ToArray())
            {
                string childPath = path + "." + pair.Key;
                string newKey = pair.Key;
                if (paths.Contains((childPath, true)))
                {
                    newKey = rewrite(childPath, true, pair.Key);
                    if (newKey != pair.Key && value.ContainsKey(newKey))
                        throw new InvalidDataException($"The renamed reference conflicts with an existing dictionary key: {childPath}");
                    rewritten.Add((childPath, true));
                }
                if (paths.Contains((childPath, false)) && JsonScalar.String(pair.Value) is string text)
                {
                    value[pair.Key] = rewrite(childPath, false, text);
                    rewritten.Add((childPath, false));
                }
                else if (pair.Value is not null)
                    rewriteIndexedValues(pair.Value, childPath, paths, rewrite, rewritten);
                if (newKey != pair.Key)
                {
                    JsonNode? child = value[pair.Key];
                    value.Remove(pair.Key);
                    value[newKey] = child;
                }
            }
        }
        else if (node is JsonArray array)
        {
            for (int index = 0; index < array.Count; index++)
            {
                string childPath = path + "[" + index + "]";
                if (paths.Contains((childPath, false)) && JsonScalar.String(array[index]) is string text)
                {
                    array[index] = rewrite(childPath, false, text);
                    rewritten.Add((childPath, false));
                }
                else if (array[index] is JsonNode child)
                    rewriteIndexedValues(child, childPath, paths, rewrite, rewritten);
            }
        }
    }

    private static string renameReferenceValue(string value, string section, string oldKey, string newKey)
    {
        if (value == oldKey)
            return newKey;
        string text = value.Trim();
        char? quote = text.Length >= 2 && text[0] == text[^1] && text[0] is '\'' or '"' ? text[0] : null;
        string content = quote is not null ? text[1..^1] : text;
        if (section == "UI" && content.StartsWith("Project:", StringComparison.Ordinal))
            content = "Project:" + UiAssetSchema.ToLogicalAssetKey(newKey);
        else if (section == "Blueprints" && BlueprintReference.IsReference(content))
            content = BlueprintReference.ToReference(newKey);
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
