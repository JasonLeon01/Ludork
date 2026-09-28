using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    private long catalogRevision = -1;
    private IReadOnlyList<MapCatalogEntry> catalogEntries = [];
    private IReadOnlyList<string> allMapKeys = [];
    private readonly Dictionary<string, MapCatalogEntry> catalogByKey = new(StringComparer.Ordinal);
    private readonly Dictionary<string, IReadOnlyDictionary<string, MapCatalogEntry>> worldCatalogs = new(StringComparer.Ordinal);
    private readonly Dictionary<string, IReadOnlyList<string>> worldChildKeys = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Dictionary<string, HashSet<string>>> worldTagMaps = new(StringComparer.Ordinal);
    private static readonly IReadOnlyDictionary<string, MapCatalogEntry> emptyWorldCatalog =
        new ReadOnlyDictionary<string, MapCatalogEntry>(new Dictionary<string, MapCatalogEntry>(StringComparer.Ordinal));

    private void ensureCatalogProjection()
    {
        if (catalogRevision == catalogDocuments.Revision)
            return;
        MapCatalogEntry[] entries = catalogDocuments.Values.Select(readMapCatalogEntry)
            .OfType<MapCatalogEntry>().Select(entry => entry with
            {
                LayerOrder = Array.AsReadOnly(entry.LayerOrder.ToArray()),
                ActorTags = Array.AsReadOnly(entry.ActorTags.ToArray()),
            })
            .OrderBy(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap ? 1 : 0)
            .ThenBy(entry => entry.Key, StringComparer.Ordinal).ToArray();
        catalogEntries = Array.AsReadOnly(entries);
        allMapKeys = Array.AsReadOnly(entries
            .Where(entry => entry.Kind is MapCatalogEntryKind.StandaloneMap or MapCatalogEntryKind.WorldChildMap)
            .Select(entry => entry.Key).ToArray());
        catalogByKey.Clear();
        worldCatalogs.Clear();
        worldChildKeys.Clear();
        worldTagMaps.Clear();
        foreach (MapCatalogEntry entry in entries)
            catalogByKey[getMapCatalogDataKey(entry.Kind, entry.Key)] = entry;
        foreach (IGrouping<string, MapCatalogEntry> world in entries
                     .Where(entry => entry.Kind == MapCatalogEntryKind.WorldChildMap && entry.WorldKey is not null)
                     .GroupBy(entry => entry.WorldKey!, StringComparer.Ordinal))
        {
            Dictionary<string, MapCatalogEntry> children = world.ToDictionary(entry => entry.Key, StringComparer.Ordinal);
            worldCatalogs[world.Key] = new ReadOnlyDictionary<string, MapCatalogEntry>(children);
            worldChildKeys[world.Key] = Array.AsReadOnly(children.Keys.ToArray());
            Dictionary<string, HashSet<string>> tags = new(StringComparer.Ordinal);
            foreach (MapCatalogEntry child in children.Values)
            {
                foreach (string tag in child.ActorTags)
                {
                    if (!tags.TryGetValue(tag, out HashSet<string>? maps))
                        tags[tag] = maps = new HashSet<string>(StringComparer.Ordinal);
                    maps.Add(child.Key);
                }
            }
            worldTagMaps[world.Key] = tags;
        }
        catalogRevision = catalogDocuments.Revision;
    }

    internal IReadOnlyDictionary<string, MapCatalogEntry> getWorldCatalog(string worldKey)
    {
        ensureCatalogProjection();
        return worldCatalogs.GetValueOrDefault(worldKey) ?? emptyWorldCatalog;
    }

    internal IReadOnlyList<string> getWorldChildKeys(string worldKey)
    {
        ensureCatalogProjection();
        return worldChildKeys.GetValueOrDefault(worldKey) ?? [];
    }

    internal bool hasWorldActorTag(string worldKey, string tag, string ignoredMapKey)
    {
        ensureCatalogProjection();
        return worldTagMaps.TryGetValue(worldKey, out Dictionary<string, HashSet<string>>? tags)
            && tags.TryGetValue(tag, out HashSet<string>? maps)
            && (maps.Count > 1 || maps.Count == 1 && !maps.Contains(ignoredMapKey));
    }
}
