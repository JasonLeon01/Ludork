using Ludork.Models;
using Ludork.Views.Utils.BlueprintGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService : IDisposable
{
    private const string BlueprintPrefix = "Data.Blueprints.";
    private static readonly string[] AudioExtensions = [".wav", ".ogg", ".mp3", ".flac", ".aac", ".m4a"];
    private static readonly IReadOnlyDictionary<string, string> DataRoots =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["config"] = "Configs",
            ["tileset"] = "Tilesets",
            ["autoTile"] = "AutoTiles",
            ["map"] = "Maps",
            ["worldMap"] = "Maps",
            ["commonFunction"] = "CommonFunctions",
            ["animation"] = "Animations",
            ["curve"] = "Curves",
            ["textConfig"] = "TextConfigs",
            ["uiAsset"] = "UI",
            ["general"] = "General",
        };

    private readonly GameDataService gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly Dictionary<string, ReferenceNode> nodes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<ReferenceRecord>> referencesBySource = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<ReferenceRecord>> referencedByTarget = new(StringComparer.Ordinal);
    private readonly Dictionary<string, IReadOnlyList<ReferenceRecord>> mapReferenceCache = new(StringComparer.Ordinal);
    private readonly HashSet<ReferenceRecord> seen = [];
    private bool allWorldChildMapReferencesBuilt;
    private bool dirty = true;
    private bool disposed;

    public ReferenceIndexService(
        GameDataService gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        gameData.DataChanged += onDataChanged;
        gameData.DataReloaded += onDataChanged;
        gameData.DataRestored += onDataChanged;
        gameData.MapPreviewChanged += onMapPreviewChanged;
    }

    public void MarkDirty()
    {
        dirty = true;
    }

    public void Rebuild()
    {
        HashSet<string> worldChildKeys = gameData.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap)
            .Select(entry => entry.Key)
            .ToHashSet(StringComparer.Ordinal);
        foreach (string staleKey in mapReferenceCache.Keys
                     .Where(key => !worldChildKeys.Contains(key))
                     .ToArray())
        {
            mapReferenceCache.Remove(staleKey);
        }
        nodes.Clear();
        referencesBySource.Clear();
        referencedByTarget.Clear();
        seen.Clear();
        buildNodes();
        buildEdges();
        dirty = false;
    }

    public string? GetNodeIdForPath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return null;
        string absolutePath;
        string relativePath;
        try
        {
            absolutePath = Path.GetFullPath(path);
            relativePath = Path.GetRelativePath(gameData.ProjectPath, absolutePath);
        }
        catch (Exception exception) when (exception is ArgumentException or NotSupportedException or PathTooLongException)
        {
            return null;
        }
        if (Path.IsPathRooted(relativePath)
            || relativePath.Equals("..", StringComparison.Ordinal)
            || relativePath.StartsWith(".." + Path.DirectorySeparatorChar, StringComparison.Ordinal)
            || relativePath.StartsWith("../", StringComparison.Ordinal))
        {
            return null;
        }

        string normalized = relativePath.Replace('\\', '/');
        string lower = normalized.ToLowerInvariant();
        if (Directory.Exists(absolutePath)
            && lower.StartsWith("data/maps/", StringComparison.Ordinal))
        {
            string worldKey = normalized["Data/Maps/".Length..].Trim('/');
            if (!worldKey.Contains('/') && gameData.WorldMapData.ContainsKey(worldKey))
                return nodeId("worldMap", worldKey);
        }
        if (lower.StartsWith("assets/", StringComparison.Ordinal))
        {
            string assetNodeId = nodeId("asset", normalizeExplicitAssetPath(normalized));
            ensureBuilt();
            ensureNode(assetNodeId);
            return assetNodeId;
        }

        if (!lower.StartsWith("data/", StringComparison.Ordinal)
            || DataConfig.isAnimationCache(normalized)
            || !string.Equals(Path.GetExtension(normalized), DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        string[] parts = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length < 3)
            return null;
        string section = parts[1];
        string key = string.Join('/', parts.Skip(2));
        key = key[..^DataConfig.DataFileExtension.Length];
        if (section.Equals("Blueprints", StringComparison.OrdinalIgnoreCase))
        {
            return gameData.BlueprintsData.ContainsKey(key)
                ? blueprintNodeIdFromKey(key)
                : null;
        }

        if (section.Equals("Maps", StringComparison.OrdinalIgnoreCase)
            && key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
        {
            string worldKey = key[..^"/_world".Length];
            return gameData.WorldMapData.ContainsKey(worldKey)
                ? nodeId("worldMap", worldKey)
                : null;
        }

        (string Type, IReadOnlyDictionary<string, JsonObject> Data)? dataSection = getDataSection(section);
        return dataSection is not null && dataSection.Value.Data.ContainsKey(key)
            ? nodeId(dataSection.Value.Type, key)
            : null;
    }

    public string GetNodePath(string nodeIdValue)
    {
        ReferenceNode? node = GetNode(nodeIdValue);
        if (node is null)
            return string.Empty;
        if (node.Type == "asset")
            return Path.Combine(gameData.ProjectPath, node.Key.Replace('/', Path.DirectorySeparatorChar));
        if (node.Type == "blueprint")
        {
            string key = node.Key.StartsWith(BlueprintPrefix, StringComparison.Ordinal)
                ? node.Key[BlueprintPrefix.Length..].Replace('.', '/')
                : node.Key;
            return dataPath("Blueprints", key);
        }
        if (node.Type == "generalMember")
        {
            string key = node.Key.Split('/', 2)[0];
            return dataPath("General", key);
        }
        if (node.Type == "worldMap")
        {
            return Path.Combine(
                gameData.ProjectPath,
                "Data",
                "Maps",
                node.Key.Replace('/', Path.DirectorySeparatorChar),
                "_world" + DataConfig.DataFileExtension);
        }
        return DataRoots.TryGetValue(node.Type, out string? root)
            ? dataPath(root, node.Key)
            : string.Empty;
    }

    public ReferenceNode? GetNode(string nodeIdValue)
    {
        ensureBuilt();
        if (nodes.TryGetValue(nodeIdValue, out ReferenceNode? node))
            return node;
        if (!nodeIdValue.Contains(':', StringComparison.Ordinal))
            return null;
        ensureNode(nodeIdValue);
        return nodes[nodeIdValue];
    }

    public IReadOnlyList<ReferenceRecord> GetIncoming(string nodeIdValue)
    {
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return referencedByTarget.TryGetValue(nodeIdValue, out List<ReferenceRecord>? records)
            ? sortRecords(records, record => record.Source)
            : [];
    }

    public IReadOnlyList<ReferenceRecord> GetOutgoing(string nodeIdValue)
    {
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return referencesBySource.TryGetValue(nodeIdValue, out List<ReferenceRecord>? records)
            ? sortRecords(records, record => record.Target)
            : [];
    }

    public ReferenceTreeNode GetTree(
        string nodeIdValue,
        ReferenceDirection direction,
        int maxDepth = 5)
    {
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        ensureNode(nodeIdValue);
        return buildTree(
            nodeIdValue,
            direction,
            Math.Max(0, maxDepth),
            new HashSet<string>(StringComparer.Ordinal) { nodeIdValue });
    }

    public ReferenceImpact GetImpactForPaths(IEnumerable<string> paths)
    {
        HashSet<string> nodeIds = new(StringComparer.Ordinal);
        foreach (string path in paths)
        {
            if (Directory.Exists(path))
            {
                foreach (string file in Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories))
                {
                    string? nestedId = GetNodeIdForPath(file);
                    if (nestedId is not null)
                        nodeIds.Add(nestedId);
                }
                continue;
            }
            string? nodeIdValue = GetNodeIdForPath(path);
            if (nodeIdValue is not null)
                nodeIds.Add(nodeIdValue);
        }
        List<ReferenceRecord> incoming = nodeIds
            .SelectMany(GetIncoming)
            .Where(record => !nodeIds.Contains(record.Source))
            .Distinct()
            .OrderBy(record => GetNode(record.Source)?.Type, StringComparer.Ordinal)
            .ThenBy(record => GetNode(record.Source)?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal)
            .ToList();
        return new ReferenceImpact(nodeIds.OrderBy(value => value, StringComparer.Ordinal).ToArray(), incoming);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        gameData.DataChanged -= onDataChanged;
        gameData.DataReloaded -= onDataChanged;
        gameData.DataRestored -= onDataChanged;
        gameData.MapPreviewChanged -= onMapPreviewChanged;
    }

    private void ensureBuilt()
    {
        if (dirty)
            Rebuild();
    }

    private void onDataChanged(object? sender, EventArgs args)
    {
        MarkDirty();
    }

    private void onMapPreviewChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        if (args.MapKey is null)
            mapReferenceCache.Clear();
        else
            mapReferenceCache.Remove(args.MapKey);
        allWorldChildMapReferencesBuilt = false;
        MarkDirty();
    }

    private void buildNodes()
    {
        addSectionNodes("config", gameData.SystemConfigData.Keys);
        addSectionNodes("tileset", gameData.TilesetData.Keys);
        addSectionNodes("autoTile", gameData.AutoTileData.Keys);
        addSectionNodes(
            "map",
            gameData.MapCatalog
                .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap)
                .Select(entry => entry.Key));
        addSectionNodes("worldMap", gameData.WorldMapData.Keys);
        addSectionNodes("commonFunction", gameData.CommonFunctionsData.Keys);
        foreach (string key in gameData.BlueprintsData.Keys)
            addNode("blueprint", BlueprintPrefix + key.Replace('/', '.'));
        addSectionNodes("animation", gameData.AnimationsData.Keys);
        addSectionNodes("curve", gameData.CurvesData.Keys);
        addSectionNodes("textConfig", gameData.TextConfigsData.Keys);
        addSectionNodes("uiAsset", gameData.UiAssetsData.Keys);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.GeneralData)
        {
            addNode("general", pair.Key);
            if (pair.Value["members"] is not JsonObject members)
                continue;
            foreach (string memberKey in members.Select(entry => entry.Key))
                addNode("generalMember", $"{pair.Key}/{memberKey}");
        }
    }

    private void addSectionNodes(string type, IEnumerable<string> keys)
    {
        foreach (string key in keys)
            addNode(type, key);
    }

    private string addNode(string type, string key)
    {
        string id = nodeId(type, key);
        nodes[id] = new ReferenceNode(id, type, key.Replace('\\', '/'));
        return id;
    }

    private void ensureNode(string id)
    {
        if (nodes.ContainsKey(id))
            return;
        int separator = id.IndexOf(':');
        if (separator < 0)
        {
            nodes[id] = new ReferenceNode(id, "unknown", id);
            return;
        }
        nodes[id] = new ReferenceNode(id, id[..separator], id[(separator + 1)..]);
    }

    private void buildEdges()
    {
        BlueprintNodeDefinitionSet globalDefinitions =
            BlueprintNodeDefinitionCatalog.CreateGlobal(metadataService, classResolver).GetNodeDefinitionSet();
        foreach (KeyValuePair<string, JsonObject> pair in gameData.SystemConfigData)
            scanConfigReferences(nodeId("config", pair.Key), pair.Key, pair.Value);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.TilesetData)
            addAssetReference(nodeId("tileset", pair.Key), pair.Value["fileName"], "Tilesets", "asset", "fileName");
        foreach (KeyValuePair<string, JsonObject> pair in gameData.AutoTileData)
            addAssetReference(nodeId("autoTile", pair.Key), pair.Value["fileName"], "Autotiles", "asset", "fileName");
        foreach (MapCatalogEntry entry in gameData.MapCatalog
                     .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap))
        {
            if (entry.Kind == MapCatalogEntryKind.WorldChildMap
                && !gameData.LoadedMapData.ContainsKey(entry.Key))
            {
                if (mapReferenceCache.TryGetValue(entry.Key, out IReadOnlyList<ReferenceRecord>? cached))
                    replayMapReferences(cached);
                continue;
            }
            scanAndCacheMapReferences(entry);
        }
        allWorldChildMapReferencesBuilt = gameData.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap)
            .All(entry => mapReferenceCache.ContainsKey(entry.Key));
        foreach (KeyValuePair<string, JsonObject> pair in gameData.WorldMapData)
            scanWorldMapReferences(nodeId("worldMap", pair.Key), pair.Key, pair.Value);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.CommonFunctionsData)
        {
            string sourceId = nodeId("commonFunction", pair.Key);
            scanNodeGraphReferences(sourceId, pair.Value, $"CommonFunctions/{pair.Key}", globalDefinitions);
            scanGenericReferences(sourceId, pair.Value, $"CommonFunctions/{pair.Key}");
        }
        foreach (KeyValuePair<string, JsonObject> pair in gameData.BlueprintsData)
            scanBlueprintReferences(pair.Key, pair.Value);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.AnimationsData)
            scanAnimationReferences(nodeId("animation", pair.Key), pair.Value, pair.Key);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.CurvesData)
            scanGenericReferences(nodeId("curve", pair.Key), pair.Value, $"Curves/{pair.Key}");
        foreach (KeyValuePair<string, JsonObject> pair in gameData.TextConfigsData)
            scanTextConfigReferences(nodeId("textConfig", pair.Key), pair.Key, pair.Value);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.UiAssetsData)
        {
            scanUiAssetReferences(
                nodeId("uiAsset", pair.Key),
                pair.Key,
                pair.Value);
        }
        foreach (KeyValuePair<string, JsonObject> pair in gameData.GeneralData)
            scanGeneralReferences(pair.Key, pair.Value, globalDefinitions);
    }

    private void addAssetReference(
        string sourceId,
        JsonNode? value,
        string baseDirectory,
        string kind,
        string path)
    {
        string assetPath = normalizeAssetPath(value, baseDirectory);
        if (assetPath.Length != 0)
            addReference(sourceId, nodeId("asset", assetPath), kind, path);
    }

    private void addReference(string sourceId, string targetId, string kind, string path)
    {
        if (sourceId.Length == 0 || targetId.Length == 0 || sourceId == targetId)
            return;
        ensureNode(sourceId);
        ensureNode(targetId);
        ReferenceRecord record = new(sourceId, targetId, kind, path);
        if (!seen.Add(record))
            return;
        if (!referencesBySource.TryGetValue(sourceId, out List<ReferenceRecord>? outgoing))
        {
            outgoing = [];
            referencesBySource[sourceId] = outgoing;
        }
        outgoing.Add(record);
        if (!referencedByTarget.TryGetValue(targetId, out List<ReferenceRecord>? incoming))
        {
            incoming = [];
            referencedByTarget[targetId] = incoming;
        }
        incoming.Add(record);
    }

    private ReferenceTreeNode buildTree(
        string currentId,
        ReferenceDirection direction,
        int depth,
        IReadOnlySet<string> stack)
    {
        IReadOnlyList<ReferenceRecord> records = direction == ReferenceDirection.ReferencedBy
            ? GetIncoming(currentId)
            : GetOutgoing(currentId);
        List<ReferenceTreeItem> items = [];
        foreach (ReferenceRecord record in records)
        {
            string childId = direction == ReferenceDirection.ReferencedBy ? record.Source : record.Target;
            bool cycle = stack.Contains(childId);
            ReferenceTreeNode child = new(childId, [], cycle);
            if (!cycle && depth > 0)
            {
                HashSet<string> childStack = new(stack, StringComparer.Ordinal) { childId };
                child = buildTree(childId, direction, depth - 1, childStack);
            }
            items.Add(new ReferenceTreeItem(record, child));
        }
        return new ReferenceTreeNode(currentId, items, false);
    }

    private IReadOnlyList<ReferenceRecord> sortRecords(
        IEnumerable<ReferenceRecord> records,
        Func<ReferenceRecord, string> nodeSelector)
    {
        return records
            .OrderBy(record => GetNode(nodeSelector(record))?.Type, StringComparer.Ordinal)
            .ThenBy(record => GetNode(nodeSelector(record))?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal)
            .ToArray();
    }

    private (string Type, IReadOnlyDictionary<string, JsonObject> Data)? getDataSection(string section)
    {
        if (section.Equals("Configs", StringComparison.OrdinalIgnoreCase))
            return ("config", gameData.SystemConfigData);
        if (section.Equals("Tilesets", StringComparison.OrdinalIgnoreCase))
            return ("tileset", gameData.TilesetData);
        if (section.Equals("AutoTiles", StringComparison.OrdinalIgnoreCase))
            return ("autoTile", gameData.AutoTileData);
        if (section.Equals("Maps", StringComparison.OrdinalIgnoreCase))
            return ("map", gameData.MapData);
        if (section.Equals("CommonFunctions", StringComparison.OrdinalIgnoreCase))
            return ("commonFunction", gameData.CommonFunctionsData);
        if (section.Equals("Animations", StringComparison.OrdinalIgnoreCase))
            return ("animation", gameData.AnimationsData);
        if (section.Equals("Curves", StringComparison.OrdinalIgnoreCase))
            return ("curve", gameData.CurvesData);
        if (section.Equals("TextConfigs", StringComparison.OrdinalIgnoreCase))
            return ("textConfig", gameData.TextConfigsData);
        if (section.Equals("UI", StringComparison.OrdinalIgnoreCase))
            return ("uiAsset", gameData.UiAssetsData);
        if (section.Equals("General", StringComparison.OrdinalIgnoreCase))
            return ("general", gameData.GeneralData);
        return null;
    }

    private string dataPath(string root, string key)
    {
        return Path.Combine(
            gameData.ProjectPath,
            "Data",
            root,
            key.Replace('/', Path.DirectorySeparatorChar) + DataConfig.DataFileExtension);
    }

    private static string nodeId(string type, string key)
    {
        return $"{type}:{key.Replace('\\', '/')}";
    }

    private static string blueprintNodeIdFromKey(string key)
    {
        return nodeId("blueprint", BlueprintPrefix + key.Replace('/', '.'));
    }

    private static string? blueprintNodeIdFromClassPath(JsonNode? value)
    {
        string? text = getString(value)?.Trim();
        return text is not null && text.StartsWith(BlueprintPrefix, StringComparison.Ordinal)
            ? nodeId("blueprint", text)
            : null;
    }

    private static JsonNode? parameterAt(JsonArray parameters, int index)
    {
        return index >= 0 && index < parameters.Count ? parameters[index] : null;
    }

    private static string? getMetaReference(JsonNode? value, string name)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out string? text))
                return text;
            if (scalar.TryGetValue(out bool enabled) && enabled)
                return string.Empty;
            return null;
        }
        if (value is JsonObject objectValue)
        {
            if (objectValue.TryGetPropertyValue(name, out JsonNode? named))
                return getString(named) ?? string.Empty;
            return null;
        }
        if (value is not JsonArray array)
            return null;
        foreach (JsonNode? item in array)
        {
            if (string.Equals(getString(item), name, StringComparison.Ordinal))
                return string.Empty;
            if (item is JsonArray tuple
                && tuple.Count != 0
                && string.Equals(getString(tuple[0]), name, StringComparison.Ordinal))
            {
                return tuple.Count > 1 ? getString(tuple[1]) ?? string.Empty : string.Empty;
            }
        }
        return null;
    }

    private static bool hasMetaReference(JsonNode? value, string name)
    {
        return getMetaReference(value, name) is not null;
    }

    private static void collectPathVariables(
        JsonNode? value,
        IDictionary<string, string> target)
    {
        if (value is JsonObject objectValue)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in objectValue)
                target[pair.Key] = getString(pair.Value) ?? "Characters";
            return;
        }
        if (value is not JsonArray array)
            return;
        foreach (JsonNode? item in array)
        {
            if (getString(item) is string name)
            {
                target[name] = "Characters";
                continue;
            }
            if (item is JsonArray tuple
                && tuple.Count != 0
                && getString(tuple[0]) is string tupleName)
            {
                target[tupleName] = tuple.Count > 1
                    ? getString(tuple[1]) ?? string.Empty
                    : string.Empty;
            }
        }
    }

    private static string normalizeAssetPath(JsonNode? value, string baseDirectory)
    {
        string? text = normalizeReferenceParam(value);
        if (string.IsNullOrWhiteSpace(text))
            return string.Empty;
        text = text.Replace('\\', '/').TrimStart('/');
        if (text.StartsWith("./", StringComparison.Ordinal))
            text = text[2..];
        if (text.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase))
            return "Assets/" + text[7..].Trim('/');
        string normalizedBase = baseDirectory.Replace('\\', '/').Trim('/');
        return normalizedBase.Length == 0
            ? "Assets/" + text.Trim('/')
            : $"Assets/{normalizedBase}/{text.Trim('/')}";
    }

    private static string normalizeExplicitAssetPath(string value)
    {
        string text = value.Replace('\\', '/').Trim();
        if (text.StartsWith("./", StringComparison.Ordinal))
            text = text[2..];
        return text.StartsWith("Assets/", StringComparison.OrdinalIgnoreCase)
            ? "Assets/" + text[7..].Trim('/')
            : string.Empty;
    }

    private static string? normalizeReferenceParam(JsonNode? value)
    {
        string? text = getString(value)?.Trim();
        if (string.IsNullOrWhiteSpace(text))
            return null;
        if (text.Length >= 2
            && text[0] == text[^1]
            && text[0] is '\'' or '"')
        {
            text = text[1..^1].Trim();
        }
        return text.Length == 0 ? null : text;
    }

    private static string normalizeDataReference(string value, string section)
    {
        string normalized = value.Replace('\\', '/').Trim().Trim('/');
        string dottedPrefix = "Data." + section + ".";
        if (normalized.StartsWith(dottedPrefix, StringComparison.Ordinal))
            normalized = normalized[dottedPrefix.Length..].Replace('.', '/');
        string slashPrefix = section + "/";
        if (normalized.StartsWith(slashPrefix, StringComparison.OrdinalIgnoreCase))
            normalized = normalized[slashPrefix.Length..];
        if (normalized.EndsWith(DataConfig.DataFileExtension, StringComparison.OrdinalIgnoreCase))
            normalized = normalized[..^DataConfig.DataFileExtension.Length];
        return normalized;
    }

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static bool isAudioAsset(string? value)
    {
        return !string.IsNullOrWhiteSpace(value)
            && AudioExtensions.Contains(Path.GetExtension(value), StringComparer.OrdinalIgnoreCase);
    }

    private sealed record MapReferenceRewrite(string MapKey, JsonObject Original, bool WasLoaded);
}
