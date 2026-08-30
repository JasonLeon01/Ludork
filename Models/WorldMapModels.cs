using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public enum MapCatalogEntryKind
{
    StandaloneMap,
    WorldMap,
    WorldChildMap,
}

public sealed record MapCatalogEntry(
    string Key,
    string DisplayName,
    MapCatalogEntryKind Kind,
    string? WorldKey,
    int Width,
    int Height,
    IReadOnlyList<string> LayerOrder,
    IReadOnlyList<string> ActorTags);

public readonly record struct WorldMapRect(int X, int Y, int Width, int Height)
{
    public bool Intersects(WorldMapRect other)
    {
        return (long)X < (long)other.X + other.Width
            && (long)X + Width > other.X
            && (long)Y < (long)other.Y + other.Height
            && (long)Y + Height > other.Y;
    }
}

public sealed record WorldMapPlacement(string Map, WorldMapRect Rect);

public sealed class WorldMapInfo
{
    public string DirectoryName { get; set; } = string.Empty;
    public string WorldName { get; set; } = string.Empty;
    public int Width { get; set; } = 13;
    public int Height { get; set; } = 13;
    public string Fog { get; set; } = string.Empty;
    public int FogPower { get; set; }
    public double FogOx { get; set; }
    public double FogOy { get; set; }
    public int FogDistort { get; set; }
    public IReadOnlyList<string> LayerOrder { get; set; } = [];
    public IReadOnlyList<WorldMapPlacement> Placements { get; set; } = [];
}

public sealed record WorldMapValidationIssue(string Code, string Message, string? Map = null);

public sealed class WorldMapValidationResult
{
    public WorldMapValidationResult(
        IReadOnlyList<WorldMapValidationIssue> issues,
        IReadOnlyList<string> layerOrder,
        IReadOnlyList<WorldMapPlacement> placements)
    {
        Issues = issues;
        LayerOrder = layerOrder;
        Placements = placements;
    }

    public bool IsValid => Issues.Count == 0;
    public IReadOnlyList<WorldMapValidationIssue> Issues { get; }
    public IReadOnlyList<string> LayerOrder { get; }
    public IReadOnlyList<WorldMapPlacement> Placements { get; }
}

public sealed record WorldMapTarget(
    string WorldKey,
    string ManifestRuntimePath,
    string? ChildMapKey,
    int OffsetX,
    int OffsetY);

public sealed record WorldMapMutationResult(bool Success, string Details)
{
    public static WorldMapMutationResult Succeeded { get; } = new(true, string.Empty);

    public static WorldMapMutationResult Failed(string details)
    {
        return new WorldMapMutationResult(false, details);
    }
}

public sealed class MapPreviewChangedEventArgs(string? mapKey) : EventArgs
{
    public string? MapKey { get; } = mapKey;
}
