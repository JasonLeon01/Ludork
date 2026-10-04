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
    private readonly Func<ProjectEnumCatalog> getProjectEnums;
    private ProjectEnumCatalog projectEnums = new(new Dictionary<string, JsonObject>(), [], []);
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
        Func<ProjectEnumCatalog> getProjectEnums,
        CancellationToken cancellationToken = default)
    {
        this.metadataService = metadataService;
        this.getProjectEnums = getProjectEnums;
        this.classResolver = classResolver;
        this.cancellationToken = cancellationToken;
        globalDefinitions = new BlueprintNodeDefinitionCatalog(metadataService, classResolver);
        blueprintDefinitions = new BlueprintNodeDefinitionCatalog(metadataService, classResolver);
    }

    public DocumentReferenceResult Scan(string section, string key, JsonObject data)
    {
        cancellationToken.ThrowIfCancellationRequested();
        projectEnums = getProjectEnums();
        references.Clear();
        seen.Clear();
        generalMemberTypes.Clear();
        using IDisposable? metadataRead = section is "Maps" or "Blueprints" or "CommonFunctions" or "General"
            ? metadataService.BeginReferenceRead()
            : null;
        switch (section)
        {
            case "Configs":
                scanConfigReferences(ReferenceIdentity.NodeId("config", key), key, data);
                break;
            case "Tilesets":
                addAssetReference(ReferenceIdentity.NodeId("tileset", key), data["fileName"], "asset", "fileName");
                break;
            case "AutoTiles":
                addAssetReference(ReferenceIdentity.NodeId("autoTile", key), data["fileName"], "asset", "fileName");
                break;
            case "Maps":
                scanMapReferences(ReferenceIdentity.NodeId("map", key), key, data);
                break;
            case "WorldMaps":
                scanWorldMapReferences(ReferenceIdentity.NodeId("worldMap", key), key, data);
                break;
            case "CommonFunctions":
                string sourceId = ReferenceIdentity.NodeId("commonFunction", key);
                if (data["nodeGraph"] is JsonObject)
                    scanNodeGraphReferences(sourceId, data, $"CommonFunctions/{key}", getGlobalDefinitions());
                scanGenericReferences(sourceId, data, $"CommonFunctions/{key}");
                break;
            case "Blueprints":
                scanBlueprintReferences(key, data);
                break;
            case "Animations":
                scanAnimationReferences(ReferenceIdentity.NodeId("animation", key), data, key);
                break;
            case "Particles":
                scanParticleReferences(key, data);
                break;
            case "Curves":
                scanGenericReferences(ReferenceIdentity.NodeId("curve", key), data, $"Curves/{key}");
                break;
            case "TextConfigs":
                scanTextConfigReferences(ReferenceIdentity.NodeId("textConfig", key), key, data);
                break;
            case "UI":
                scanUiAssetReferences(ReferenceIdentity.NodeId("uiAsset", key), key, data);
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
            addReference(sourceId, ReferenceIdentity.NodeId("asset", assetPath), kind, path);
    }

    private void addSubtitleReference(string sourceId, JsonNode? value, string kind, string path)
    {
        if (GameDataPath.TryGetSubtitleKey(JsonScalar.String(value), out string key))
            addReference(sourceId, ReferenceIdentity.NodeId("subtitle", key), kind, path);
    }

    private void addReference(string sourceId, string targetId, string kind, string path, bool isDictionaryKey = false)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (sourceId.Length == 0 || targetId.Length == 0 || sourceId == targetId && kind is not "generalType" and not "generalTypeValue" and not "generalTypeDependency")
            return;
        ReferenceRecord reference = new(sourceId, targetId, kind, path, isDictionaryKey);
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
            "SUBTITLES" => "subtitle",
            "CURVES" => "curve",
            "TEXTCONFIGS" => "textConfig",
            "UI" => "uiAsset",
            "GENERAL" => "general",
            _ => null,
        };
    }
}
