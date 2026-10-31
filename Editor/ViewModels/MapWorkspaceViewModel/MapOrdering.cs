using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.ViewModels;

public sealed partial class MapWorkspaceViewModel
{
    private sealed record MapDrop(string SourceScope, string SourceName, string TargetScope, string? TargetName,
        MapOrderDropPosition Position)
    {
        public bool ChangesFolder => !string.Equals(SourceScope, TargetScope, StringComparison.Ordinal);
    }

    public bool CanDropMap(MapListItemViewModel source, MapListItemViewModel? target, MapOrderDropPosition position) =>
        resolveMapDrop(source, target, position) is not null;

    public bool IsMapFolderMove(MapListItemViewModel source, MapListItemViewModel? target, MapOrderDropPosition position) =>
        resolveMapDrop(source, target, position)?.ChangesFolder == true;

    public bool ReorderMap(MapListItemViewModel source, MapListItemViewModel? target, MapOrderDropPosition position)
    {
        if (resolveMapDrop(source, target, position) is not MapDrop drop || drop.ChangesFolder
            || !GameData.MapOrder.Move(drop.SourceScope, drop.SourceName, drop.TargetName, drop.Position))
            return false;
        refreshMaps(source.Key);
        return true;
    }

    public Task<IReadOnlyList<ReferenceRecord>> MapMoveReferencesAsync(MapListItemViewModel source,
        MapListItemViewModel? target, MapOrderDropPosition position, IProgress<EditorOperationProgress>? progress = null)
    {
        if (resolveMapDrop(source, target, position) is not MapDrop { ChangesFolder: true } drop
            || drop.TargetScope.Length == 0
            || GameData.MapOrder.FindNode(drop.SourceScope, drop.SourceName) is not MapOrderNode subtree)
            return Task.FromResult<IReadOnlyList<ReferenceRecord>>([]);
        string[] paths = enumerateOrderNames(subtree)
            .Select(name => GameData.Maps.GetMapRuntimePath(MapOrderService.GetKey(drop.SourceScope, name))).ToArray();
        return references.GetExternalMapReferencesAsync(paths, progress: progress);
    }

    public async Task<WorldMapMutationResult> MoveMapAsync(MapListItemViewModel source, MapListItemViewModel? target,
        MapOrderDropPosition position, IProgress<EditorOperationProgress>? progress = null)
    {
        if (resolveMapDrop(source, target, position) is not MapDrop { ChangesFolder: true } drop
            || GameData.MapOrder.FindNode(drop.SourceScope, drop.SourceName) is not MapOrderNode subtree)
            return WorldMapMutationResult.Failed(string.Format(LocaleService.Get("MAP_MOVE_FAILED"), source.Key));
        string[] keys = enumerateOrderNames(subtree)
            .Select(name => MapOrderService.GetKey(drop.SourceScope, name)).ToArray();
        WorldMapMutationResult result = await GameData.Maps.MoveMapsAsync(keys,
            drop.TargetScope.Length == 0 ? null : drop.TargetScope, progress);
        if (!result.Success)
            return result;
        GameData.MapOrder.Transfer(drop.SourceScope, subtree, drop.TargetScope, drop.TargetName, drop.Position);
        refreshMaps(MapOrderService.GetKey(drop.TargetScope, drop.SourceName));
        return result;
    }

    private MapDrop? resolveMapDrop(MapListItemViewModel source, MapListItemViewModel? target, MapOrderDropPosition position)
    {
        if (!CanEdit || liveDebugSession is not null || ReferenceEquals(source, target)
            || target is not null && enumerateMapItems(source.Children).Contains(target))
            return null;
        string sourceScope = source.IsWorldChild ? source.WorldKey! : string.Empty;
        string sourceName = MapOrderService.GetName(source.Key);
        if (target is null)
            return new MapDrop(sourceScope, sourceName, string.Empty, null, MapOrderDropPosition.After);
        if (source.IsWorld)
            return position != MapOrderDropPosition.Inside && Maps.Contains(target)
                ? new MapDrop(sourceScope, sourceName, string.Empty, target.Key, position) : null;
        if (target.IsWorld)
            return position == MapOrderDropPosition.Inside
                ? new MapDrop(sourceScope, sourceName, target.Key, null, position)
                : new MapDrop(sourceScope, sourceName, string.Empty, target.Key, position);
        string targetScope = target.IsWorldChild ? target.WorldKey! : string.Empty;
        return new MapDrop(sourceScope, sourceName, targetScope, MapOrderService.GetName(target.Key), position);
    }

    private static IEnumerable<string> enumerateOrderNames(MapOrderNode node) =>
        node.Children.SelectMany(enumerateOrderNames).Prepend(node.Name);
}
