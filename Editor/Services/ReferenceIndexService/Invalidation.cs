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
                (change.Section is "Blueprints" or "General" or "Animations" or "Particles") && (change.ContentChanged || change.IdentityChanged)))
        {
            MarkDirty();
            return;
        }
        invalidateBackground(false);
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
            "Materials" => "material",
            "Subtitles" => "subtitle",
            "Curves" => "curve",
            "TextConfigs" => "textConfig",
            "UI" => "uiAsset",
            _ => null,
        };
        if (type is null)
            return;
        pendingDocuments[ReferenceIdentity.NodeId(type, key)] = (section, key);
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
        List<ReferenceBuildDocument> documents = [];
        List<(string Type, string Key)> declarations = [];
        foreach ((string section, string key) in pendingDocuments.Values)
        {
            if (section == "Maps")
            {
                MapCatalogEntry? entry = gameData.Maps.MapCatalog.FirstOrDefault(entry => entry.Key == key
                    && entry.Kind != MapCatalogEntryKind.WorldMap);
                if (entry is not null)
                {
                    declarations.Add(("map", key));
                    if (createMapScanDocument(entry) is ReferenceBuildDocument document)
                        documents.Add(document);
                }
                continue;
            }
            (string Type, IReadOnlyDictionary<string, JsonObject> Data)? dataSection = getDataSection(section);
            if (dataSection is null || !dataSection.Value.Data.TryGetValue(key, out JsonObject? data))
                continue;
            declarations.Add((dataSection.Value.Type, key));
            documents.Add(createScanDocument(section, key, data));
        }
        DocumentReferenceResult[] results = scanDocuments(documents);
        nodesByDocumentPath = null;
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
        foreach ((string type, string key) in declarations)
            addNode(type, key);
        for (int index = 0; index < documents.Count; index++)
            applyScannedDocument(documents[index], results[index]);
        foreach (string id in affectedNodes)
        {
            if (declaredNodes.Contains(id) || referencesBySource.ContainsKey(id) || referencedByTarget.ContainsKey(id))
                continue;
            nodes.Remove(id);
            generalMemberTypes.Remove(id);
        }
        allWorldChildMapReferencesBuilt = gameData.Maps.MapCatalog
            .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap)
            .All(entry => mapReferenceCache.ContainsKey(entry.Key));
        pendingDocuments.Clear();
        metadataRevision = metadataService.Revision;
        dirty = false;
    }

}
