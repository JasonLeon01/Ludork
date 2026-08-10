using Ludork.Models;
using Ludork.Views.Utils.BlueprintGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class ReferenceIndexService : IDisposable
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
    private readonly HashSet<ReferenceRecord> seen = [];
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
    }

    public void MarkDirty()
    {
        dirty = true;
    }

    public void Rebuild()
    {
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
        return referencedByTarget.TryGetValue(nodeIdValue, out List<ReferenceRecord>? records)
            ? sortRecords(records, record => record.Source)
            : [];
    }

    public IReadOnlyList<ReferenceRecord> GetOutgoing(string nodeIdValue)
    {
        ensureBuilt();
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

    private void buildNodes()
    {
        addSectionNodes("config", gameData.SystemConfigData.Keys);
        addSectionNodes("tileset", gameData.TilesetData.Keys);
        addSectionNodes("autoTile", gameData.AutoTileData.Keys);
        addSectionNodes("map", gameData.MapData.Keys);
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
        IReadOnlyList<BlueprintGraphNodeDefinition> globalDefinitions =
            BlueprintNodeDefinitionCatalog.CreateGlobal(metadataService, classResolver).GetNodeDefinitions();
        foreach (KeyValuePair<string, JsonObject> pair in gameData.SystemConfigData)
            scanConfigReferences(nodeId("config", pair.Key), pair.Key, pair.Value);
        foreach (KeyValuePair<string, JsonObject> pair in gameData.TilesetData)
            addAssetReference(nodeId("tileset", pair.Key), pair.Value["fileName"], "Tilesets", "asset", "fileName");
        foreach (KeyValuePair<string, JsonObject> pair in gameData.AutoTileData)
            addAssetReference(nodeId("autoTile", pair.Key), pair.Value["fileName"], "Autotiles", "asset", "fileName");
        foreach (KeyValuePair<string, JsonObject> pair in gameData.MapData)
            scanMapReferences(nodeId("map", pair.Key), pair.Key, pair.Value);
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

    private void scanTextConfigReferences(string sourceId, string key, JsonObject data)
    {
        addAssetReference(
            sourceId,
            data["font"],
            "Fonts",
            "font",
            $"TextConfigs/{key}.font");
        if (data["gradient"] is JsonObject gradient
            && normalizeReferenceParam(gradient["curve"]) is string curve
            && curve.Length != 0)
        {
            addReference(
                sourceId,
                nodeId("curve", normalizeDataReference(curve, "Curves")),
                "curve",
                $"TextConfigs/{key}.gradient.curve");
        }
    }

    private void scanUiAssetReferences(
        string sourceId,
        string key,
        JsonObject data)
    {
        if (data["root"] is JsonObject root)
            scanUiNodeReferences(sourceId, key, root, "root");
    }

    private void scanUiNodeReferences(
        string sourceId,
        string key,
        JsonObject node,
        string path)
    {
        string? controlId = getString(node["controlId"]);
        if (controlId is not null
            && UiAssetSchema.TryGetProjectAssetKey(controlId, out string targetAssetKey))
        {
            string targetKey = UiAssetSchema.ToAssetDataKey(targetAssetKey);
            if (gameData.UiAssetsData.ContainsKey(targetKey))
            {
                addReference(
                    sourceId,
                    nodeId("uiAsset", targetKey),
                    "nestedUiAsset",
                    $"UI/{key}.{path}.controlId");
            }
        }
        if (node["properties"] is JsonObject properties)
        {
            addAssetReference(
                sourceId,
                properties["texture"],
                string.Empty,
                "uiResource",
                $"UI/{key}.{path}.properties.texture");
            addAssetReference(
                sourceId,
                properties["windowSkin"],
                string.Empty,
                "uiResource",
                $"UI/{key}.{path}.properties.windowSkin");
            if (normalizeReferenceParam(properties["textConfig"]) is string textConfig
                && textConfig.Length != 0)
            {
                addReference(
                    sourceId,
                    nodeId("textConfig", normalizeDataReference(textConfig, "TextConfigs")),
                    "textConfig",
                    $"UI/{key}.{path}.properties.textConfig");
            }
            if (normalizeReferenceParam(properties["opacityCurve"]) is string curve
                && curve.Length != 0)
            {
                addReference(
                    sourceId,
                    nodeId("curve", normalizeDataReference(curve, "Curves")),
                    "curve",
                    $"UI/{key}.{path}.properties.opacityCurve");
            }
        }
        if (node["children"] is not JsonArray children)
            return;
        for (int index = 0; index < children.Count; index++)
        {
            if (children[index] is JsonObject child)
            {
                scanUiNodeReferences(
                    sourceId,
                    key,
                    child,
                    $"{path}.children[{index}]");
            }
        }
    }

    private void scanConfigReferences(string sourceId, string key, JsonObject data)
    {
        foreach (KeyValuePair<string, JsonNode?> pair in data)
        {
            if (pair.Value is not JsonObject setting)
                continue;
            string? valueType = getString(setting["type"]);
            if (valueType is null || !valueType.StartsWith("file", StringComparison.Ordinal))
                continue;
            JsonArray values = setting["value"] is JsonArray array
                ? array
                : new JsonArray(setting["value"]?.DeepClone());
            string root = getString(setting["root"]) ?? "Assets";
            string baseDirectory = getString(setting["base"]) ?? string.Empty;
            for (int index = 0; index < values.Count; index++)
            {
                string path = $"Configs/{key}.{pair.Key}[{index}]";
                if (root.Equals("Data", StringComparison.OrdinalIgnoreCase)
                    && baseDirectory.Equals("Maps", StringComparison.OrdinalIgnoreCase))
                {
                    addMapReference(sourceId, values[index], "configFile", path);
                }
                else
                {
                    addAssetReference(sourceId, values[index], baseDirectory, "configFile", path);
                }
            }
        }
    }

    private void scanMapReferences(string sourceId, string key, JsonObject data)
    {
        if (data["layers"] is JsonObject layers)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in layers)
            {
                if (pair.Value is not JsonObject layer)
                    continue;
                string? tilesetKey = getString(layer["layerTileset"]);
                if (!string.IsNullOrWhiteSpace(tilesetKey))
                {
                    addReference(
                        sourceId,
                        nodeId("tileset", tilesetKey),
                        "tileset",
                        $"Maps/{key}.layers.{pair.Key}.layerTileset");
                }
                addAssetReference(
                    sourceId,
                    layer["shaderPath"],
                    "Shaders",
                    "asset",
                    $"Maps/{key}.layers.{pair.Key}.shaderPath");
                scanAutoTileReferences(
                    sourceId,
                    layer["autoTiles"],
                    $"Maps/{key}.layers.{pair.Key}.autoTiles");
                scanMapActorReferences(
                    sourceId,
                    layer["actors"],
                    $"Maps/{key}.layers.{pair.Key}.actors");
            }
        }
        if (data["actors"] is JsonObject actorsByLayer)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in actorsByLayer)
                scanMapActorReferences(sourceId, pair.Value, $"Maps/{key}.actors.{pair.Key}");
        }
        else
        {
            scanMapActorReferences(sourceId, data["actors"], $"Maps/{key}.actors");
        }
        addAssetReference(sourceId, data["bgm"], "Musics", "asset", $"Maps/{key}.bgm");
        addAssetReference(sourceId, data["bgs"], "Musics", "asset", $"Maps/{key}.bgs");
        addAssetReference(sourceId, data["fog"], "Fogs", "asset", $"Maps/{key}.fog");
        scanGenericReferences(sourceId, data["BPClassVarChanged"], $"Maps/{key}.BPClassVarChanged");
    }

    private void scanAutoTileReferences(string sourceId, JsonNode? value, string path)
    {
        string? key = getString(value);
        if (!string.IsNullOrWhiteSpace(key))
        {
            addReference(sourceId, nodeId("autoTile", key), "autoTile", path);
            return;
        }
        if (value is not JsonArray array)
            return;
        for (int index = 0; index < array.Count; index++)
            scanAutoTileReferences(sourceId, array[index], $"{path}[{index}]");
    }

    private void scanMapActorReferences(string sourceId, JsonNode? value, string path)
    {
        if (value is not JsonArray actors)
            return;
        for (int index = 0; index < actors.Count; index++)
        {
            if (actors[index] is not JsonObject actor)
                continue;
            string? blueprintId = blueprintNodeIdFromClassPath(actor["bp"]);
            if (blueprintId is not null)
                addReference(sourceId, blueprintId, "mapActor", $"{path}[{index}].bp");
            scanGenericReferences(sourceId, actor, $"{path}[{index}]");
        }
    }

    private void scanBlueprintReferences(string key, JsonObject data)
    {
        string sourceId = blueprintNodeIdFromKey(key);
        string? parentId = blueprintNodeIdFromClassPath(data["parent"]);
        if (parentId is not null)
            addReference(sourceId, parentId, "parent", $"Blueprints/{key}.parent");

        if (data["attrs"] is JsonObject attrs)
        {
            Dictionary<string, string> pathVariables = new(StringComparer.Ordinal)
            {
                ["texturePath"] = "Characters",
                ["shaderPath"] = "Shaders",
            };
            ResolvedBlueprintClass resolved = classResolver.ResolveBlueprint(data, key);
            collectPathVariables(resolved.Meta["PathVars"], pathVariables);
            foreach (ResolvedBlueprintField field in resolved.Fields)
            {
                if (field.Metadata is null)
                    continue;
                if (getMetaReference(field.Metadata.Meta["PathRoot"], field.Name) == "Project")
                    continue;
                string? baseDirectory = getMetaReference(field.Metadata.Meta["PathVars"], field.Name);
                if (baseDirectory is not null)
                    pathVariables[field.Name] = baseDirectory;
            }
            foreach (KeyValuePair<string, string> pair in pathVariables)
            {
                if (attrs.ContainsKey(pair.Key))
                {
                    addAssetReference(
                        sourceId,
                        attrs[pair.Key],
                        pair.Value,
                        "asset",
                        $"Blueprints/{key}.attrs.{pair.Key}");
                }
            }
            scanGenericReferences(sourceId, attrs, $"Blueprints/{key}.attrs");
        }

        if (data["graph"] is JsonObject graph)
        {
            BlueprintGraphContext context = new(data, key);
            IReadOnlyList<BlueprintGraphNodeDefinition> definitions =
                new BlueprintNodeDefinitionCatalog(metadataService, classResolver, context).GetNodeDefinitions();
            scanNodeGraphReferences(sourceId, graph, $"Blueprints/{key}.graph", definitions);
            scanGenericReferences(sourceId, graph, $"Blueprints/{key}.graph");
        }
    }

    private void scanAnimationReferences(string sourceId, JsonObject data, string key)
    {
        if (data["assets"] is JsonArray assets)
        {
            for (int index = 0; index < assets.Count; index++)
            {
                string? assetName = getString(assets[index]);
                string baseDirectory = isAudioAsset(assetName) ? "Sounds" : "Animations";
                addAssetReference(sourceId, assets[index], baseDirectory, "animationAsset", $"assets[{index}]");
            }
        }
        scanGenericReferences(sourceId, data, $"Animations/{key}");
    }

    private void scanGeneralReferences(
        string key,
        JsonObject data,
        IReadOnlyList<BlueprintGraphNodeDefinition> globalDefinitions)
    {
        string sourceId = nodeId("general", key);
        JsonObject parameterSchema = data["params"] as JsonObject ?? [];
        if (data["members"] is not JsonObject members)
            return;
        foreach (KeyValuePair<string, JsonNode?> pair in members)
        {
            string memberId = nodeId("generalMember", $"{key}/{pair.Key}");
            addReference(sourceId, memberId, "member", $"General/{key}.members.{pair.Key}");
            if (pair.Value is not JsonObject member)
                continue;
            addAssetReference(memberId, member["icon"], string.Empty, "asset", "icon");
            scanGeneralParameterReferences(memberId, key, pair.Key, member, parameterSchema);
            if (member["_graph"] is JsonObject graph)
            {
                scanNodeGraphReferences(
                    memberId,
                    graph,
                    $"General/{key}/{pair.Key}._graph",
                    globalDefinitions);
                scanGenericReferences(memberId, graph, $"General/{key}/{pair.Key}._graph");
            }
        }
    }

    private void scanGeneralParameterReferences(
        string sourceId,
        string dataKey,
        string memberKey,
        JsonObject member,
        JsonObject schema)
    {
        foreach (KeyValuePair<string, JsonNode?> pair in schema)
        {
            if (pair.Value is not JsonObject definition)
                continue;
            string type = getString(definition["type"]) ?? "string";
            if (type is not "string" and not "list" and not "dict")
                continue;
            JsonNode? value = member[pair.Key];
            if (type == "list" && value is JsonArray list)
            {
                for (int index = 0; index < list.Count; index++)
                    addGeneralParameterReference(sourceId, definition, list[index], $"General/{dataKey}/{memberKey}.{pair.Key}[{index}]");
                continue;
            }
            if (type == "dict" && value is JsonObject dictionary)
            {
                foreach (string itemKey in dictionary.Select(entry => entry.Key))
                    addGeneralParameterReference(sourceId, definition, JsonValue.Create(itemKey), $"General/{dataKey}/{memberKey}.{pair.Key}.{itemKey}");
                continue;
            }
            addGeneralParameterReference(sourceId, definition, value, $"General/{dataKey}/{memberKey}.{pair.Key}");
        }
    }

    private void addGeneralParameterReference(
        string sourceId,
        JsonObject definition,
        JsonNode? value,
        string path)
    {
        if (definition["reference"] is not JsonObject reference)
            return;
        string? kind = getString(reference["kind"]);
        string? text = normalizeReferenceParam(value);
        if (string.IsNullOrWhiteSpace(text))
            return;
        if (kind == "animation")
        {
            addReference(sourceId, nodeId("animation", text), "reference", path);
            return;
        }
        if (kind == "general" && getString(reference["key"]) is string generalKey)
            addReference(sourceId, nodeId("generalMember", $"{generalKey}/{text}"), "member", path);
    }

    private void scanNodeGraphReferences(
        string sourceId,
        JsonObject graphData,
        string path,
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions)
    {
        if (graphData["nodeGraph"] is not JsonObject nodeGraph)
            return;
        IReadOnlyDictionary<string, BlueprintGraphNodeDefinition> lookup =
            BlueprintNodeDefinitionCatalog.CreateDefinitionLookup(definitions);
        foreach (KeyValuePair<string, JsonNode?> graphPair in nodeGraph)
        {
            if (graphPair.Value is not JsonObject graph || graph["nodes"] is not JsonArray graphNodes)
                continue;
            for (int index = 0; index < graphNodes.Count; index++)
            {
                if (graphNodes[index] is not JsonObject node)
                    continue;
                string nodePath = $"{path}.nodeGraph.{graphPair.Key}.nodes[{index}]";
                string? nodeFunction = getString(node["nodeFunction"]);
                JsonArray parameters = node["params"] as JsonArray ?? [];
                if (nodeFunction is not null && lookup.TryGetValue(nodeFunction, out BlueprintGraphNodeDefinition? definition))
                    scanDefinitionParameterReferences(sourceId, definition, parameters, nodePath);
                scanKnownNodeParameterReferences(sourceId, nodeFunction, parameters, nodePath);
                scanGenericReferences(sourceId, parameters, $"{nodePath}.params");
            }
        }
    }

    private void scanDefinitionParameterReferences(
        string sourceId,
        BlueprintGraphNodeDefinition definition,
        JsonArray parameters,
        string path)
    {
        foreach (BlueprintGraphPortDefinition port in definition.Ports)
        {
            if (port.Direction != BlueprintGraphPortDirection.Input
                || port.Kind != BlueprintGraphPortKind.Params
                || port.ParameterIndex is not int parameterIndex
                || parameterIndex < 0
                || parameterIndex >= parameters.Count)
            {
                continue;
            }
            JsonNode? value = parameters[parameterIndex];
            string referencePath = $"{path}.params[{parameterIndex}]";
            if (getMetaReference(port.Meta["PathVars"], port.Name) is string baseDirectory)
            {
                addAssetReference(sourceId, value, baseDirectory, "nodeParam", referencePath);
            }
            if (hasMetaReference(port.Meta["BlueprintClassVars"], port.Name))
            {
                string? blueprintId = blueprintNodeIdFromClassPath(value);
                if (blueprintId is not null)
                    addReference(sourceId, blueprintId, "nodeParam", referencePath);
            }
            if (hasMetaReference(port.Meta["CommonFunctionVars"], port.Name)
                && normalizeReferenceParam(value) is string functionName)
            {
                addReference(sourceId, nodeId("commonFunction", functionName), "nodeParam", referencePath);
            }
            if (getMetaReference(port.Meta["GeneralDataVars"], port.Name) is string generalType)
            {
                string? generalValue = normalizeReferenceParam(value);
                if (generalValue is not null)
                {
                    string targetId = generalType.Equals("ANIMATION", StringComparison.OrdinalIgnoreCase)
                        ? nodeId("animation", generalValue)
                        : nodeId("generalMember", $"{generalType}/{generalValue}");
                    addReference(sourceId, targetId, "nodeParam", referencePath);
                }
            }
        }
    }

    private void scanKnownNodeParameterReferences(
        string sourceId,
        string? nodeFunction,
        JsonArray parameters,
        string path)
    {
        if (string.IsNullOrWhiteSpace(nodeFunction))
            return;
        if (nodeFunction.EndsWith(".GotoMap", StringComparison.Ordinal))
            addMapReference(sourceId, parameterAt(parameters, 0), "nodeParam", $"{path}.params[0]");

        (string[] Suffixes, int Parameter, string Type, string Base)[] rules =
        [
            ([".AddPlayerByClass", ".RemovePlayerByClass", ".CreateActorFromBPPath", ".CreateActorFromBPPathWithDefaults"], 0, "blueprint", ""),
            ([".AddAnim", ".AddAnimOn", ".GetAnimLength"], 0, "animation", ""),
            ([".RunCommonFunction"], 0, "commonFunction", ""),
            ([".PlaySound"], 0, "asset", "Sounds"),
            ([".ShowVoiceMessageByTag", ".ShowVoiceMessage"], 2, "asset", "Voices"),
            ([".PlayMusic"], 0, "asset", "Musics"),
            ([".PlayVideo"], 0, "asset", "Videos"),
            ([".GetItemCount", ".AddItem", ".RemoveItem", ".HasItem"], 0, "generalMember", "Item"),
            ([".AddEquip", ".RemoveEquip", ".HasEquip", ".EquipItem"], 0, "generalMember", "Equip"),
        ];
        foreach ((string[] suffixes, int parameter, string type, string baseValue) in rules)
        {
            if (!suffixes.Any(suffix => nodeFunction.EndsWith(suffix, StringComparison.Ordinal)))
                continue;
            JsonNode? value = parameterAt(parameters, parameter);
            string referencePath = $"{path}.params[{parameter}]";
            string? text = normalizeReferenceParam(value);
            if (string.IsNullOrWhiteSpace(text))
                continue;
            if (type == "blueprint")
            {
                string? blueprintId = blueprintNodeIdFromClassPath(value);
                if (blueprintId is not null)
                    addReference(sourceId, blueprintId, "nodeParam", referencePath);
            }
            else if (type == "asset")
            {
                addAssetReference(sourceId, value, baseValue, "nodeParam", referencePath);
            }
            else if (type == "generalMember")
            {
                addReference(sourceId, nodeId("generalMember", $"{baseValue}/{text}"), "nodeParam", referencePath);
            }
            else
            {
                addReference(sourceId, nodeId(type, text), "nodeParam", referencePath);
            }
        }
    }

    private void scanGenericReferences(string sourceId, JsonNode? value, string path)
    {
        if (getString(value) is string text)
        {
            string? blueprintId = blueprintNodeIdFromClassPath(value);
            if (blueprintId is not null)
            {
                addReference(sourceId, blueprintId, "blueprintPath", path);
                return;
            }
            string assetPath = normalizeExplicitAssetPath(text);
            if (assetPath.Length != 0)
                addReference(sourceId, nodeId("asset", assetPath), "asset", path);
            return;
        }
        if (value is JsonObject objectValue)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in objectValue)
                scanGenericReferences(sourceId, pair.Value, $"{path}.{pair.Key}");
            return;
        }
        if (value is not JsonArray arrayValue)
            return;
        for (int index = 0; index < arrayValue.Count; index++)
            scanGenericReferences(sourceId, arrayValue[index], $"{path}[{index}]");
    }

    private void addMapReference(string sourceId, JsonNode? value, string kind, string path)
    {
        string? key = normalizeReferenceParam(value);
        if (string.IsNullOrWhiteSpace(key))
            return;
        key = Path.ChangeExtension(key.Replace('\\', '/'), null) ?? key;
        addReference(sourceId, nodeId("map", key), kind, path);
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
}
