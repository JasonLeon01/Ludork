using System.Collections.Generic;

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

public sealed record WorldMapValidationIssue(string Code, string Message, string? Map = null);

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
