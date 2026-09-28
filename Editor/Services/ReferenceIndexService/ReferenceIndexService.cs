using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService : IDisposable
{
    private readonly DocumentReferenceScanner scanner;
    private DocumentReferenceScanner? transactionScanner;
    private readonly ProjectDataStore gameData;
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly CancellationToken cancellationToken;
    private readonly Action<string>? progress;
    private readonly Dictionary<string, ReferenceNode> nodes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, string> generalMemberTypes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<ReferenceRecord>> referencesBySource = new(StringComparer.Ordinal);
    private readonly Dictionary<string, List<ReferenceRecord>> referencedByTarget = new(StringComparer.Ordinal);
    private readonly Dictionary<string, IReadOnlyList<ReferenceRecord>> mapReferenceCache = new(StringComparer.Ordinal);
    private readonly HashSet<ReferenceRecord> seen = [];
    private readonly HashSet<string> declaredNodes = new(StringComparer.Ordinal);
    private readonly Dictionary<string, (string Section, string Key)> pendingDocuments = new(StringComparer.Ordinal);
    private long metadataRevision = -1;
    private bool allWorldChildMapReferencesBuilt;
    private bool dirty = true;
    private bool disposed;

    public ReferenceIndexService(
        ProjectDataStore gameData,
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        CancellationToken cancellationToken = default,
        Action<string>? progress = null)
    {
        this.gameData = gameData;
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.cancellationToken = cancellationToken;
        this.progress = progress;
        scanner = new DocumentReferenceScanner(metadataService, classResolver, cancellationToken);
        gameData.Documents.ContentInvalidated += onContentInvalidated;
    }

    public void MarkDirty()
    {
        dirty = true;
        invalidateBackground(true);
    }

    public void Rebuild()
    {
        MarkDirty();
        ensureBuilt();
    }

    private void rebuildIndex()
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        dirty = true;
        nodesByDocumentPath = null;
        mapReferenceCache.Clear();
        pendingDocuments.Clear();
        declaredNodes.Clear();
        nodes.Clear();
        generalMemberTypes.Clear();
        referencesBySource.Clear();
        referencedByTarget.Clear();
        seen.Clear();
        buildNodes();
        buildEdges();
        metadataRevision = metadataService.Revision;
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
            if (!worldKey.Contains('/') && gameData.Worlds.WorldMapData.ContainsKey(worldKey))
                return ReferenceIdentity.NodeId("worldMap", worldKey);
        }
        if (lower.StartsWith("assets/", StringComparison.Ordinal))
        {
            if (gameData.GetDocumentByPath(absolutePath) is { Section: "Subtitles" } subtitle)
                return ReferenceIdentity.NodeId("subtitle", subtitle.Key);
            if (!GameAssetPath.TryFromProjectFile(
                    gameData.ProjectPath,
                    absolutePath,
                    out string logicalPath))
            {
                return null;
            }
            string assetNodeId = ReferenceIdentity.NodeId("asset", logicalPath);
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
            return gameData.Blueprints.BlueprintsData.ContainsKey(key)
                ? ReferenceIdentity.BlueprintNodeId(key)
                : null;
        }

        if (section.Equals("Maps", StringComparison.OrdinalIgnoreCase)
            && key.EndsWith("/_world", StringComparison.OrdinalIgnoreCase))
        {
            string worldKey = key[..^"/_world".Length];
            return gameData.Worlds.WorldMapData.ContainsKey(worldKey)
                ? ReferenceIdentity.NodeId("worldMap", worldKey)
                : null;
        }

        if (section.Equals("Maps", StringComparison.OrdinalIgnoreCase))
            return gameData.Maps.containsMapKey(key) ? ReferenceIdentity.NodeId("map", key) : null;
        (string Type, IReadOnlyDictionary<string, JsonObject> Data)? dataSection = getDataSection(section);
        return dataSection is not null && dataSection.Value.Data.ContainsKey(key)
            ? ReferenceIdentity.NodeId(dataSection.Value.Type, key)
            : null;
    }

    public string GetNodePath(string nodeIdValue)
    {
        if (nodeIdValue.StartsWith("generalMember:", StringComparison.Ordinal) && !generalMemberTypes.ContainsKey(nodeIdValue))
            ensureBuilt();
        return ReferenceIndexSnapshot.ResolvePath(gameData.ProjectPath, nodeIdValue, generalMemberTypes);
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
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return referencedByTarget.TryGetValue(nodeIdValue, out List<ReferenceRecord>? records)
            ? sortRecords(records, record => record.Source)
            : [];
    }

    public IReadOnlyList<ReferenceRecord> GetOutgoing(string nodeIdValue)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return referencesBySource.TryGetValue(nodeIdValue, out List<ReferenceRecord>? records)
            ? sortRecords(records, record => record.Target)
            : [];
    }

    public IReadOnlyList<ReferenceNode> GetAllNodes()
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return nodes.Values.OrderBy(node => node.Id, StringComparer.Ordinal).ToArray();
    }

    public IReadOnlyList<ReferenceRecord> GetAllReferences()
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return seen.OrderBy(record => record.Source, StringComparer.Ordinal)
            .ThenBy(record => record.Target, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal).ToArray();
    }

    public IReadOnlyList<ReferenceRecord> GetOutgoingForDocumentPath(string path)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        return sortRecords(
            getNodeIdsForDocumentPath(path, false)
                .SelectMany(id => referencesBySource.GetValueOrDefault(id) ?? [])
                .Distinct(),
            record => record.Target);
    }

    public ReferenceTreeNode GetTree(
        string nodeIdValue,
        ReferenceDirection direction,
        int maxDepth = 5)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        ensureNode(nodeIdValue);
        ReferenceIndexSnapshot snapshot = new(gameData.ProjectPath, version, nodes.Values, seen, generalMemberTypes, declaredNodes);
        return snapshot.GetTree(nodeIdValue, direction, maxDepth);
    }

    public ReferenceImpact GetImpactForPaths(IEnumerable<string> paths)
    {
        using IDisposable metadataRead = metadataService.BeginRead();
        ensureBuilt();
        ensureAllWorldChildMapReferences();
        HashSet<string> nodeIds = new(StringComparer.Ordinal);
        foreach (string path in paths)
        {
            string? nodeIdValue = GetNodeIdForPath(path);
            if (nodeIdValue is not null)
                nodeIds.Add(nodeIdValue);
            nodeIds.UnionWith(getNodeIdsForDocumentPath(path, true));
        }
        List<ReferenceRecord> incoming = nodeIds
            .SelectMany(id => referencedByTarget.GetValueOrDefault(id) ?? [])
            .Where(record => !nodeIds.Contains(record.Source))
            .Distinct()
            .OrderBy(record => nodes.GetValueOrDefault(record.Source)?.Type, StringComparer.Ordinal)
            .ThenBy(record => nodes.GetValueOrDefault(record.Source)?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal)
            .ToList();
        return new ReferenceImpact(nodeIds.OrderBy(value => value, StringComparer.Ordinal).ToArray(), incoming);
    }

    private IReadOnlyList<string> getNodeIdsForDocumentPath(string path, bool includeDescendants)
    {
        string fullPath;
        try
        {
            fullPath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(path));
        }
        catch (Exception exception) when (exception is ArgumentException or NotSupportedException or PathTooLongException)
        {
            return [];
        }
        return findDocumentNodes(fullPath, includeDescendants);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        disposeBackground();
        gameData.Documents.ContentInvalidated -= onContentInvalidated;
    }

    private void ensureBuilt()
    {
        ObjectDisposedException.ThrowIf(disposed, this);
        activate();
        if (gameData.Documents.HasPendingNotifications)
        {
            using BlueprintClassResolver resolver = new(metadataService, gameData.ReadReferenceBlueprint);
            transactionScanner = new DocumentReferenceScanner(metadataService, resolver, cancellationToken);
            try
            {
                ReferenceInputFiles transactionalFiles = ReferenceInputFiles.Capture(gameData.ProjectPath, unloadedMapPaths());
                rebuildIndex();
                ensureAllWorldChildMapReferences();
                if (!transactionalFiles.IsCurrent())
                    throw new IOException("Reference inputs changed during indexing.");
            }
            finally
            {
                transactionScanner = null;
                dirty = true;
            }
            return;
        }
        if (!dirty && (metadataRevision != metadataService.Revision || inputFiles is not null && !inputFiles.IsCurrent()))
            MarkDirty();
        if (!dirty && pendingDocuments.Count == 0 && CurrentSnapshot?.Version == version)
            return;
        ReferenceInputFiles files = ReferenceInputFiles.Capture(gameData.ProjectPath, unloadedMapPaths());
        if (dirty)
            rebuildIndex();
        else
            updatePendingDocuments();
        ensureAllWorldChildMapReferences();
        if (!files.IsCurrent())
        {
            MarkDirty();
            throw new IOException("Reference inputs changed during indexing.");
        }
        inputFiles = files;
        observedFiles = files;
        backgroundCancellation?.Cancel();
        publish(new ReferenceIndexSnapshot(gameData.ProjectPath, version, nodes.Values, seen, generalMemberTypes, declaredNodes));
    }

    private void buildNodes()
    {
        addSectionNodes("config", gameData.Configs.SystemConfigData.Keys);
        addSectionNodes("tileset", gameData.Assets.TilesetData.Keys);
        addSectionNodes("autoTile", gameData.Assets.AutoTileData.Keys);
        addSectionNodes(
            "map",
            gameData.Maps.MapCatalog
                .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap)
                .Select(entry => entry.Key));
        addSectionNodes("worldMap", gameData.Worlds.WorldMapData.Keys);
        addSectionNodes("commonFunction", gameData.Blueprints.CommonFunctionsData.Keys);
        foreach (string key in gameData.Blueprints.BlueprintsData.Keys)
            addNode("blueprint", BlueprintReference.ToReference(key));
        addSectionNodes("animation", gameData.Assets.AnimationsData.Keys);
        addSectionNodes("particle", gameData.Assets.ParticlesData.Keys);
        addSectionNodes("subtitle", gameData.Subtitles.Keys);
        addSectionNodes("curve", gameData.Assets.CurvesData.Keys);
        addSectionNodes("textConfig", gameData.Assets.TextConfigsData.Keys);
        addSectionNodes("uiAsset", gameData.UiAssets.UiAssetsData.Keys);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.GetReferenceSection("General"))
        {
            addNode("general", pair.Key);
            if (pair.Value["members"] is not JsonObject members)
                continue;
            foreach (string memberKey in members.Select(entry => entry.Key))
            {
                string memberId = generalMemberNodeId(pair.Key, memberKey);
                declaredNodes.Add(memberId);
                ensureNode(memberId);
            }
        }
    }

    private void addSectionNodes(string type, IEnumerable<string> keys)
    {
        foreach (string key in keys)
            addNode(type, key);
    }

    private string addNode(string type, string key)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string id = ReferenceIdentity.NodeId(type, key);
        nodesByDocumentPath = null;
        declaredNodes.Add(id);
        nodes[id] = ReferenceIndexSnapshot.ParseNode(id)!;
        return id;
    }

    private void ensureNode(string id)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (nodes.ContainsKey(id))
            return;
        nodesByDocumentPath = null;
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
        foreach ((string section, string key, JsonObject data) in gameData.ReferenceDocuments)
        {
            if (section != "Maps")
                scanDocumentReferences(section, key, data);
        }
        foreach (MapCatalogEntry entry in gameData.Maps.MapCatalog.Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap))
        {
            if (entry.Kind != MapCatalogEntryKind.WorldChildMap || gameData.Maps.LoadedMapData.ContainsKey(entry.Key))
                scanAndCacheMapReferences(entry);
        }
        allWorldChildMapReferencesBuilt = gameData.Maps.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap)
            .All(entry => mapReferenceCache.ContainsKey(entry.Key));
    }

    private void scanDocumentReferences(string section, string key, JsonObject data)
    {
        cancellationToken.ThrowIfCancellationRequested();
        progress?.Invoke($"Data/{section}/{key}.json");
        DocumentReferenceResult result = (transactionScanner ?? scanner).Scan(section, key, data);
        foreach (KeyValuePair<string, string> member in result.GeneralMemberTypes)
            generalMemberTypes[member.Key] = member.Value;
        foreach (ReferenceRecord reference in result.References)
            addReference(reference.Source, reference.Target, reference.Kind, reference.Path);
    }

    private void addReference(string sourceId, string targetId, string kind, string path)
    {
        if (sourceId.Length == 0 || targetId.Length == 0 || sourceId == targetId && kind != "generalType")
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

    private IReadOnlyList<ReferenceRecord> sortRecords(
        IEnumerable<ReferenceRecord> records,
        Func<ReferenceRecord, string> nodeSelector)
    {
        return records
            .OrderBy(record => nodes.GetValueOrDefault(nodeSelector(record))?.Type, StringComparer.Ordinal)
            .ThenBy(record => nodes.GetValueOrDefault(nodeSelector(record))?.Key, StringComparer.Ordinal)
            .ThenBy(record => record.Path, StringComparer.Ordinal)
            .ToArray();
    }

    private (string Type, IReadOnlyDictionary<string, JsonObject> Data)? getDataSection(string section)
    {
        foreach (KeyValuePair<string, string> root in ReferenceIndexSnapshot.DataRoots)
            if (root.Value.Equals(section, StringComparison.OrdinalIgnoreCase))
                return (root.Key, gameData.GetReferenceSection(root.Value));
        return null;
    }

    private string dataPath(string root, string key)
    {
        return Path.Combine(
            gameData.getSectionRoot(root),
            key.Replace('/', Path.DirectorySeparatorChar) + DataConfig.DataFileExtension);
    }

    private string generalMemberNodeId(string typeKey, string memberKey)
    {
        string id = ReferenceIdentity.GeneralMemberNodeId(typeKey, memberKey);
        generalMemberTypes[id] = typeKey;
        return id;
    }

}
