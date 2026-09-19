using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class ReferenceIndexService
{
    private void onContentInvalidated(object? sender, EditorDocumentsChangedEventArgs args)
    {
        if (args.Reset || args.Changes.Any(change =>
                (change.Section is "Blueprints" or "General") && (change.ContentChanged || change.IdentityChanged)))
        {
            MarkDirty();
            return;
        }
        if (dirty)
            return;
        foreach (EditorDocumentChange change in args.Changes)
        {
            if (!change.ContentChanged && !change.IdentityChanged)
                continue;
            if (change.PreviousKey is string previousKey)
                queueDocument(change.Section, previousKey);
            if (change.Key is string key)
                queueDocument(change.Section, key);
        }
    }

    private void queueDocument(string section, string key)
    {
        string? type = section switch
        {
            "Configs" => "config",
            "Tilesets" => "tileset",
            "AutoTiles" => "autoTile",
            "Maps" => "map",
            "WorldMaps" => "worldMap",
            "CommonFunctions" => "commonFunction",
            "Animations" => "animation",
            "Particles" => "particle",
            "Curves" => "curve",
            "TextConfigs" => "textConfig",
            "UI" => "uiAsset",
            _ => null,
        };
        if (type is null)
            return;
        pendingDocuments[nodeId(type, key)] = (section, key);
        if (section == "Maps")
        {
            mapReferenceCache.Remove(key);
            allWorldChildMapReferencesBuilt = false;
        }
    }

    private void updatePendingDocuments()
    {
        if (pendingDocuments.Count == 0)
            return;
        using IDisposable metadataRead = metadataService.BeginRead();
        if (metadataRevision != metadataService.CacheRevision)
        {
            Rebuild();
            return;
        }
        dirty = true;
        HashSet<string> affectedNodes = new(StringComparer.Ordinal);
        foreach (string sourceId in pendingDocuments.Keys)
        {
            declaredNodes.Remove(sourceId);
            affectedNodes.Add(sourceId);
            if (!referencesBySource.Remove(sourceId, out List<ReferenceRecord>? outgoing))
                continue;
            foreach (ReferenceRecord record in outgoing)
            {
                seen.Remove(record);
                affectedNodes.Add(record.Target);
                if (!referencedByTarget.TryGetValue(record.Target, out List<ReferenceRecord>? incoming))
                    continue;
                incoming.Remove(record);
                if (incoming.Count == 0)
                    referencedByTarget.Remove(record.Target);
            }
        }
        BlueprintNodeDefinitionSet? globalDefinitions = null;
        foreach (KeyValuePair<string, (string Section, string Key)> pending in pendingDocuments)
        {
            (string section, string key) = pending.Value;
            if (section == "Maps")
            {
                MapCatalogEntry? entry = gameData.MapCatalog.FirstOrDefault(entry => entry.Key == key
                    && entry.Kind != MapCatalogEntryKind.WorldMap);
                if (entry is not null)
                {
                    addNode("map", key);
                    scanAndCacheMapReferences(entry);
                }
                continue;
            }
            (string Type, IReadOnlyDictionary<string, JsonObject> Data)? dataSection = getDataSection(section);
            if (dataSection is null || !dataSection.Value.Data.TryGetValue(key, out JsonObject? data))
                continue;
            addNode(dataSection.Value.Type, key);
            if (section == "CommonFunctions")
                globalDefinitions ??= new BlueprintNodeDefinitionCatalog(metadataService, classResolver).GetNodeDefinitionSet();
            scanDocumentReferences(section, key, data, globalDefinitions);
        }
        foreach (string id in affectedNodes)
        {
            if (declaredNodes.Contains(id) || referencesBySource.ContainsKey(id) || referencedByTarget.ContainsKey(id))
                continue;
            nodes.Remove(id);
            generalMemberTypes.Remove(id);
        }
        allWorldChildMapReferencesBuilt = gameData.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap)
            .All(entry => mapReferenceCache.ContainsKey(entry.Key));
        pendingDocuments.Clear();
        metadataRevision = metadataService.Revision;
        dirty = false;
    }

    private void scanDocumentReferences(string section, string key, JsonObject data,
        BlueprintNodeDefinitionSet? globalDefinitions)
    {
        cancellationToken.ThrowIfCancellationRequested();
        progress?.Invoke($"Data/{section}/{key}.json");
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
            case "WorldMaps":
                scanWorldMapReferences(nodeId("worldMap", key), key, data);
                break;
            case "CommonFunctions":
                string sourceId = nodeId("commonFunction", key);
                scanNodeGraphReferences(sourceId, data, $"CommonFunctions/{key}", globalDefinitions!);
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
                scanGeneralReferences(key, data, globalDefinitions!);
                break;
        }
    }
}
