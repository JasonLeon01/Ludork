using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapOrderService
{
    internal const string FileName = "_info.json";
    private IReadOnlyList<MapOrderWrite> lastWrites = [];

    internal static bool IsOrderFile(string path) =>
        string.Equals(Path.GetFileName(path), FileName, StringComparison.OrdinalIgnoreCase);

    internal string GetPath(string scope) => scope.Length == 0
        ? Path.Combine(store.ProjectPath, "Data", "Maps", FileName)
        : Path.Combine(store.ProjectPath, "Data", "Maps", scope, FileName);

    internal bool TryGetScope(string path, out string scope)
    {
        scope = string.Empty;
        if (!IsOrderFile(path) || !store.Maps.tryGetMapsRelativePath(Path.GetFullPath(path), out string relative))
            return false;
        string[] parts = relative.Split(['/', '\\'], StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length > 2)
            return false;
        scope = parts.Length == 2 ? parts[0] : string.Empty;
        return true;
    }

    internal IEnumerable<string> GetModifiedPaths() => getActiveScopes().Where(isScopeModified).Select(GetPath);

    internal void load()
    {
        trees.Clear();
        storedTexts.Clear();
        savedTexts.Clear();
        foreach (string scope in getActiveScopes())
        {
            string path = GetPath(scope);
            if (!File.Exists(path))
                continue;
            store.reportDataRead(path);
            IReadOnlyList<MapOrderNode>? tree = parse(File.ReadAllBytes(path));
            if (tree is null)
            {
                store.addInvalidLoadPath(path);
                continue;
            }
            string text = serialize(tree);
            trees[scope] = tree;
            storedTexts[scope] = text;
            savedTexts[scope] = text;
        }
    }

    internal IReadOnlyList<MapOrderNode> PrepareDiskChange(string path, byte[]? content)
    {
        if (content is null)
            return [];
        return parse(content) ?? throw new InvalidDataException($"The map order file could not be parsed: {path}");
    }

    internal void ApplyDiskChange(string scope, IReadOnlyList<MapOrderNode> tree)
    {
        string text = serialize(tree);
        bool modified = isScopeModified(scope);
        savedTexts[scope] = text;
        if (modified)
            return;
        trees[scope] = tree;
        storedTexts[scope] = text;
    }

    internal void AddToSaveBatch(EditorFileSaveBatch batch)
    {
        List<MapOrderWrite> writes = [];
        foreach (string scope in getActiveScopes().Where(isScopeModified).ToArray())
        {
            IReadOnlyList<MapOrderNode> tree = GetTree(scope);
            string text = serialize(tree);
            batch.Write(GetPath(scope), Encoding.UTF8.GetBytes(text));
            writes.Add(new MapOrderWrite(scope, tree, text, storedTexts.GetValueOrDefault(scope, emptyText)));
        }
        lastWrites = writes;
    }

    internal void MarkSaved()
    {
        foreach (MapOrderWrite write in lastWrites)
        {
            trees[write.Scope] = write.Tree;
            storedTexts[write.Scope] = write.Text;
            savedTexts[write.Scope] = write.Text;
        }
    }

    internal void CompleteSave(MapOrderService saved)
    {
        foreach (MapOrderWrite write in saved.lastWrites)
        {
            savedTexts[write.Scope] = write.Text;
            if (!string.Equals(storedTexts.GetValueOrDefault(write.Scope, emptyText), write.PreviousText, StringComparison.Ordinal))
                continue;
            trees[write.Scope] = write.Tree;
            storedTexts[write.Scope] = write.Text;
        }
    }

    private static IReadOnlyList<MapOrderNode>? parse(byte[] content)
    {
        try
        {
            if (JsonNode.Parse(Encoding.UTF8.GetString(content).TrimStart('\uFEFF')) is not JsonObject root
                || root.Count != 1
                || root["order"] is not JsonArray order)
                return null;
            return parseNodes(order);
        }
        catch (JsonException)
        {
            return null;
        }
    }

    private static IReadOnlyList<MapOrderNode>? parseNodes(JsonArray array)
    {
        List<MapOrderNode> result = [];
        foreach (JsonNode? item in array)
        {
            if (ProjectDataStore.getString(item) is string name && name.Length != 0)
            {
                result.Add(new MapOrderNode(name, []));
                continue;
            }
            if (item is not JsonObject node || node.Count != 1)
                return null;
            KeyValuePair<string, JsonNode?> entry = node.First();
            if (entry.Key.Length == 0 || entry.Value is not JsonArray children
                || parseNodes(children) is not IReadOnlyList<MapOrderNode> parsed)
                return null;
            result.Add(new MapOrderNode(entry.Key, parsed));
        }
        return result;
    }

    private static string serialize(IReadOnlyList<MapOrderNode> tree) =>
        new JsonObject { ["order"] = serializeNodes(tree) }.ToJsonString(ProjectDataStore.WriteOptions) + "\n";

    private static JsonArray serializeNodes(IReadOnlyList<MapOrderNode> nodes)
    {
        JsonArray result = new JsonArray();
        foreach (MapOrderNode node in nodes)
            result.Add(node.Children.Count == 0 ? JsonValue.Create(node.Name) : new JsonObject { [node.Name] = serializeNodes(node.Children) });
        return result;
    }
}
