using Ludork.Models;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed partial class MapWorkspaceViewModel
{
    private (string SourceKey, JsonObject Data)? mapClipboard;
    public bool CanPasteMap => mapClipboard is not null;

    public MapInfo NewMapDefaults() => new()
    {
        FileName = GameData.Maps.getNewMapFileName(),
        MapName = Ludork.Services.LocaleService.Get("NEW_MAP_DEFAULT_NAME"),
        Width = 13,
        Height = 13,
    };

    public WorldMapInfo NewWorldDefaults()
    {
        for (int index = 1; ; index++)
        {
            string name = $"World_{index:D2}";
            if (!GameData.Worlds.WorldMapData.ContainsKey(name)
                && !GameData.Maps.MapCatalog.Any(entry => string.Equals(entry.Key, name, StringComparison.Ordinal)))
                return new WorldMapInfo { DirectoryName = name, WorldName = name, Width = 256, Height = 192 };
        }
    }

    public MapInfo NewWorldChildDefaults(string worldKey)
    {
        HashSet<string> names = GameData.Worlds.getWorldChildren(worldKey).Select(Path.GetFileName)
            .Where(name => name is not null).Select(name => name!).ToHashSet(StringComparer.Ordinal);
        for (int index = 1; ; index++)
        {
            string name = $"Map_{index:D2}";
            if (!names.Contains(name))
                return new MapInfo { FileName = name + ".json", MapName = Ludork.Services.LocaleService.Get("NEW_MAP_DEFAULT_NAME"), Width = 13, Height = 13 };
        }
    }

    public bool CreateMap(MapInfo info)
    {
        if (!CanEdit || !GameData.Maps.CreateMap(info))
            return false;
        refreshMaps(normaliseMapKey(info.FileName));
        return true;
    }

    public bool CreateWorld(WorldMapInfo info)
    {
        if (!CanEdit || !GameData.Worlds.CreateWorldMap(info.DirectoryName, info))
            return false;
        refreshMaps(info.DirectoryName);
        return true;
    }

    public bool CreateWorldChild(string worldKey, MapInfo info)
    {
        if (!CanEdit)
            return false;
        info.FileName = Path.GetFileName(info.FileName);
        if (!GameData.Worlds.CreateWorldChildMap(worldKey, info))
            return false;
        refreshMaps(worldKey + "/" + normaliseMapKey(info.FileName));
        return true;
    }

    public WorldMapMutationResult UpdateWorld(string worldKey, WorldMapInfo info)
    {
        WorldMapMutationResult result = CanEdit ? GameData.Worlds.UpdateWorldMap(worldKey, info)
            : WorldMapMutationResult.Failed("The project is running.");
        if (result.Success)
            refreshMaps(worldKey);
        return result;
    }

    public IReadOnlyList<string> WorldRenameCandidates(string worldKey) => GameData.Worlds.WorldMapData.Keys
        .Where(key => !string.Equals(key, worldKey, StringComparison.Ordinal))
        .Concat(GameData.Maps.MapCatalog.Where(entry => entry.Kind == MapCatalogEntryKind.StandaloneMap).Select(entry => entry.Key)).ToArray();

    public bool RenameWorld(string worldKey, string newKey)
    {
        if (!CanEdit || !GameData.Worlds.RenameWorldMap(worldKey, newKey))
            return false;
        refreshMaps(newKey);
        return true;
    }

    public IReadOnlyList<ReferenceRecord> WorldDeleteReferences(string worldKey)
    {
        IReadOnlyList<string> children = GameData.Worlds.getWorldChildren(worldKey);
        string[] paths = children.Select(GameData.Maps.GetMapRuntimePath)
            .Append(GameData.Worlds.GetWorldManifestRuntimePath(worldKey)).ToArray();
        return references.GetExternalMapReferences(paths, children);
    }

    public int WorldChildCount(string worldKey) => GameData.Worlds.getWorldChildren(worldKey).Count;

    public bool DeleteWorld(string worldKey)
    {
        if (!CanEdit || !GameData.Worlds.DeleteWorldMap(worldKey))
            return false;
        refreshMaps();
        return true;
    }

    public (MapInfo? Info, string? WorldKey) MapProperties(string key)
    {
        MapInfo? info = GameData.Maps.getMapInfo(key);
        string? world = GameData.Worlds.TryGetWorldForMap(key, out string parent) ? parent : null;
        if (info is not null && world is not null)
            info.FileName = Path.GetFileName(info.FileName);
        return (info, world);
    }

    public bool UpdateMap(string key, MapInfo info, string? worldKey)
    {
        if (!CanEdit)
            return false;
        if (worldKey is not null)
            info.FileName = worldKey + "/" + Path.GetFileName(info.FileName);
        if (!GameData.Maps.UpdateMap(key, info))
            return false;
        refreshMaps(normaliseMapKey(info.FileName));
        return true;
    }

    public void CopyMap(string key)
    {
        if (CanEdit && GameData.Maps.ReadMapSnapshot(key) is JsonObject map)
        {
            mapClipboard = (key, map);
            OnPropertyChanged(nameof(CanPasteMap));
        }
    }

    public void PasteMap()
    {
        if (!CanEdit || mapClipboard is not { } clipboard)
            return;
        string? key = GameData.Worlds.TryGetWorldForMap(clipboard.SourceKey, out _)
            ? GameData.Worlds.CopyWorldChildMap(clipboard.SourceKey)
            : GameData.Maps.PasteMap(clipboard.Data, clipboard.SourceKey);
        if (key is not null)
            refreshMaps(key);
    }

    public IReadOnlyList<ReferenceRecord> MapDeleteReferences(string key) =>
        references.GetExternalMapReferences([GameData.Maps.GetMapRuntimePath(key)], [key]);

    public bool DeleteMap(string key)
    {
        if (!CanEdit || !GameData.Maps.DeleteMap(key))
            return false;
        refreshMaps();
        return true;
    }

    public IReadOnlyList<ReferenceRecord> PlacementRemoveReferences(string childKey) =>
        references.GetExternalMapReferences([GameData.Maps.GetMapRuntimePath(childKey)]);

    public WorldMapMutationResult UpdateWorldPlacement(string worldKey, string childKey, int x, int y) =>
        CanEdit ? GameData.Worlds.UpdateWorldPlacement(worldKey, childKey, x, y) : WorldMapMutationResult.Failed("The project is running.");

    public WorldMapMutationResult RemoveWorldPlacement(string worldKey, string childKey) =>
        CanEdit ? GameData.Worlds.RemoveWorldPlacement(worldKey, childKey) : WorldMapMutationResult.Failed("The project is running.");

    public string DescribeReferences(IReadOnlyList<ReferenceRecord> entries) => string.Join(Environment.NewLine,
        entries.Select(reference =>
        {
            ReferenceNode? source = references.GetNode(reference.Source);
            return (source is null ? reference.Source : source.Type + ":" + source.Key) + " · " + reference.Path;
        }));

    private static string normaliseMapKey(string fileName)
    {
        string key = fileName.Replace('\\', '/').Trim().Trim('/');
        return key.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ? key[..^5] : key;
    }
}
