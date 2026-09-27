using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed partial class DocumentReferenceScanner
{
    private readonly LuaMetadataService metadataService;
    private readonly BlueprintClassResolver classResolver;
    private readonly CancellationToken cancellationToken;
    private readonly BlueprintNodeDefinitionCatalog globalDefinitions;
    private readonly BlueprintNodeDefinitionCatalog blueprintDefinitions;
    private readonly List<ReferenceRecord> references = [];
    private readonly HashSet<ReferenceRecord> seen = [];
    private readonly Dictionary<string, string> generalMemberTypes = new(StringComparer.Ordinal);

    public DocumentReferenceScanner(
        LuaMetadataService metadataService,
        BlueprintClassResolver classResolver,
        CancellationToken cancellationToken = default)
    {
        this.metadataService = metadataService;
        this.classResolver = classResolver;
        this.cancellationToken = cancellationToken;
        globalDefinitions = new BlueprintNodeDefinitionCatalog(metadataService, classResolver);
        blueprintDefinitions = new BlueprintNodeDefinitionCatalog(metadataService, classResolver);
    }

    public DocumentReferenceResult Scan(string section, string key, JsonObject data)
    {
        cancellationToken.ThrowIfCancellationRequested();
        references.Clear();
        seen.Clear();
        generalMemberTypes.Clear();
        using IDisposable? metadataRead = section is "Maps" or "Blueprints" or "CommonFunctions" or "General"
            ? metadataService.BeginReferenceRead()
            : null;
        switch (section)
        {
            case "Configs":
                scanConfigReferences(nodeId("config", key), key, data);
                break;
            case "Tilesets":
                addAssetReference(nodeId("tileset", key), data["fileName"], "asset", "fileName");
                break;
            case "AutoTiles":
                addAssetReference(nodeId("autoTile", key), data["fileName"], "asset", "fileName");
                break;
            case "Maps":
                scanMapReferences(nodeId("map", key), key, data);
                break;
            case "WorldMaps":
                scanWorldMapReferences(nodeId("worldMap", key), key, data);
                break;
            case "CommonFunctions":
                string sourceId = nodeId("commonFunction", key);
                if (data["nodeGraph"] is JsonObject)
                    scanNodeGraphReferences(sourceId, data, $"CommonFunctions/{key}", getGlobalDefinitions());
                scanGenericReferences(sourceId, data, $"CommonFunctions/{key}");
                break;
            case "Blueprints":
                scanBlueprintReferences(key, data);
                break;
            case "Animations":
                scanAnimationReferences(nodeId("animation", key), data, key);
                break;
            case "Particles":
                scanParticleReferences(key, data);
                break;
            case "Curves":
                scanGenericReferences(nodeId("curve", key), data, $"Curves/{key}");
                break;
            case "TextConfigs":
                scanTextConfigReferences(nodeId("textConfig", key), key, data);
                break;
            case "UI":
                scanUiAssetReferences(nodeId("uiAsset", key), key, data);
                break;
            case "General":
                scanGeneralReferences(key, data);
                break;
        }
        return new DocumentReferenceResult(
            Array.AsReadOnly(references.ToArray()),
            new ReadOnlyDictionary<string, string>(new Dictionary<string, string>(generalMemberTypes, StringComparer.Ordinal)));
    }

    private BlueprintNodeDefinitionSet getGlobalDefinitions()
    {
        cancellationToken.ThrowIfCancellationRequested();
        return globalDefinitions.GetNodeDefinitionSet();
    }

    private void addAssetReference(string sourceId, JsonNode? value, string kind, string path)
    {
        string assetPath = normalizeAssetPath(value);
        if (assetPath.Length != 0)
            addReference(sourceId, nodeId("asset", assetPath), kind, path);
    }

    private void addReference(string sourceId, string targetId, string kind, string path)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (sourceId.Length == 0 || targetId.Length == 0 || sourceId == targetId && kind != "generalType")
            return;
        ReferenceRecord reference = new(sourceId, targetId, kind, path);
        if (seen.Add(reference))
            references.Add(reference);
    }

    private static string? getSectionType(string section)
    {
        return section.ToUpperInvariant() switch
        {
            "CONFIGS" => "config",
            "TILESETS" => "tileset",
            "AUTOTILES" => "autoTile",
            "MAPS" => "map",
            "WORLDMAPS" => "worldMap",
            "COMMONFUNCTIONS" => "commonFunction",
            "ANIMATIONS" => "animation",
            "PARTICLES" => "particle",
            "CURVES" => "curve",
            "TEXTCONFIGS" => "textConfig",
            "UI" => "uiAsset",
            "GENERAL" => "general",
            _ => null,
        };
    }
}
