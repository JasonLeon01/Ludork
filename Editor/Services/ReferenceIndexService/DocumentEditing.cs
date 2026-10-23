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
        return prepareIndexedReferenceRewrites(incoming.Where(record => record.Kind != "generalTypeDependency"),
            (record, value) => record.Kind == "generalType" && enumModuleRenames.TryGetValue(value, out string? module)
                ? module : renameReferenceValue(value, section, oldKey, newKey));
    }

    public IReadOnlyList<ReferenceRewrite> PrepareGeneralMemberRename(string typeKey, string oldId, string newId)
    {
        return prepareIndexedReferenceRewrites(GetIncoming(ReferenceIdentity.GeneralMemberNodeId(typeKey, oldId)),
            (_, value) => value == oldId ? newId
                : throw new InvalidDataException($"The indexed member reference changed: {value}"));
    }

    public IReadOnlyList<ReferenceRewrite> PrepareAssetPathRewrites(IReadOnlyDictionary<string, string> replacements)
        => PrepareAssetPathRewrites(replacements, null);

    internal IReadOnlyList<ReferenceRewrite> PrepareAssetPathRewrites(IReadOnlyDictionary<string, string> replacements,
        IReadOnlySet<(string Section, string Key)>? documents)
    {
        foreach (KeyValuePair<string, string> replacement in replacements)
            if (!GameAssetPath.IsCanonical(replacement.Key) || !GameAssetPath.IsCanonical(replacement.Value))
                throw new InvalidDataException("Asset replacements must use canonical asset paths.");
        ReferenceRecord[] incoming = replacements.Keys
            .SelectMany(path => GetIncoming(ReferenceIdentity.NodeId("asset", path))).ToArray();
        return prepareIndexedReferenceRewrites(incoming, (_, value) =>
            replacements.TryGetValue(value, out string? replacement) ? replacement
                : throw new InvalidDataException($"The indexed asset reference changed: {value}"), documents);
    }

    private IReadOnlyList<ReferenceRewrite> prepareIndexedReferenceRewrites(IEnumerable<ReferenceRecord> references,
        Func<ReferenceRecord, string, string> rewrite, IReadOnlySet<(string Section, string Key)>? documents = null)
    {
        List<ReferenceRewrite> result = [];
        foreach (IGrouping<string, ReferenceRecord> group in references.Where(record => record.StoragePath is not null)
                     .Distinct().GroupBy(record => GetNodePath(record.Source)))
        {
            EditorDocument? document = gameData.GetDocumentByPath(group.Key);
            if (documents is not null && (document is null || !documents.Contains((document.Section, document.Key))))
                continue;
            if (document?.Data is not JsonObject original)
                throw new InvalidDataException($"The referenced document could not be read: {group.Key}");
            if (group.Any(reference => reference.StoragePath == string.Empty))
                throw new InvalidDataException($"The indexed reference path is ambiguous: {group.Key}");
            JsonObject candidate = (JsonObject)original.DeepClone();
            Dictionary<(string Path, bool IsDictionaryKey), ReferenceRecord[]> locations = group
                .GroupBy(record => (record.StoragePath!, record.IsDictionaryKey))
                .ToDictionary(records => records.Key, records => records.ToArray());
            HashSet<(string Path, bool IsDictionaryKey)> paths = locations.Keys.ToHashSet();
            HashSet<(string Path, bool IsDictionaryKey)> rewritten = [];
            rewriteIndexedValues(candidate, string.Empty, paths,
                (path, isDictionaryKey, value) =>
                {
                    string[] values = locations[(path, isDictionaryKey)]
                        .Select(record => rewrite(record, value)).Distinct(StringComparer.Ordinal).ToArray();
                    if (values.Length != 1)
                        throw new InvalidDataException($"The indexed references have conflicting replacements: {path}");
                    return values[0];
                }, rewritten);
            if (!paths.SetEquals(rewritten))
                throw new InvalidDataException("The indexed reference could not be updated: "
                    + string.Join(", ", paths.Except(rewritten)));
            if (!JsonNode.DeepEquals(original, candidate))
                result.Add(new ReferenceRewrite(document.Section, document.Key, original, candidate));
        }
        return result;
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
                string childPath = path + "/" + pair.Key.Replace("~", "~0").Replace("/", "~1");
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
                string childPath = path + "/" + index;
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
            content = "Project:" + newKey;
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
