using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.Plugins;

internal sealed class MapEditorHostBridge : IMapEditorHost
{
    private readonly ProjectDataStore gameData;
    private readonly BlueprintPreviewService previewService;
    private readonly Action<string, string> refreshMap;
    private readonly Func<string, string, bool> canWriteLayer;

    public MapEditorHostBridge(
        ProjectDataStore gameData,
        BlueprintPreviewService previewService,
        string? suggestedMapKey,
        Action<string, string> refreshMap,
        Func<string, string, bool> canWriteLayer)
    {
        this.gameData = gameData;
        this.previewService = previewService;
        this.refreshMap = refreshMap;
        this.canWriteLayer = canWriteLayer;
        SuggestedMapKey = suggestedMapKey;
    }

    public string ProjectPath => gameData.ProjectPath;

    public int CellSize => gameData.Configs.getCellSize();

    public string? SuggestedMapKey { get; }

    public IPluginActorPreview AcquireActorPreview(PluginMapActorSnapshot actor)
    {
        PluginMapActorRectSnapshot rect = actor.SourceRect ?? new(0, 0, CellSize, CellSize);
        ActorVisualDescriptor descriptor = new(string.Empty, actor.AssetPath,
            new PixelSize(rect.X + rect.Width, rect.Y + rect.Height), new PixelRect(rect.X, rect.Y, rect.Width, rect.Height),
            actor.Material, (JsonObject)actor.MaterialParameters.DeepClone(), default, new Vector(1, 1), default,
            0, true, false, false, 0.2, 1);
        return new PluginActorPreview(previewService.ActorPreviews.Acquire(descriptor, 0, true));
    }

    public IReadOnlyList<PluginMapSummary> ListMaps()
    {
        return runOnUiThread(listMaps);
    }

    public PluginMapSnapshot ReadMap(string mapKey)
    {
        return runOnUiThread(() => readMap(mapKey));
    }

    public PluginTilesetSnapshot ReadTileset(string tilesetKey)
    {
        return runOnUiThread(() => readTileset(tilesetKey));
    }

    public string ResolveAssetFile(string assetPath)
    {
        return gameData.AssetPaths.TryResolveAsset(assetPath,
            out string filePath)
            ? filePath
            : string.Empty;
    }

    public async Task<PluginMapWriteResult> ReplaceLayerAndSaveAsync(
        PluginMapLayerWriteRequest request,
        CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(request);
        cancellationToken.ThrowIfCancellationRequested();
        if (Dispatcher.UIThread.CheckAccess())
            return replaceLayerAndSave(request, cancellationToken);
        return await Dispatcher.UIThread.InvokeAsync(
            () => replaceLayerAndSave(request, cancellationToken));
    }

    private IReadOnlyList<PluginMapSummary> listMaps()
    {
        List<PluginMapSummary> maps = gameData.Maps.MapCatalog
            .Where(entry => entry.Kind != MapCatalogEntryKind.WorldMap)
            .Select(entry => new PluginMapSummary(entry.Key, entry.DisplayName))
            .ToList();
        maps.Sort((left, right) => StringComparer.Ordinal.Compare(left.Key, right.Key));
        return maps.AsReadOnly();
    }

    private PluginMapSnapshot readMap(string mapKey)
    {
        if (gameData.Maps.ReadMapSnapshot(mapKey) is not JsonObject map)
            throw new KeyNotFoundException($"Map '{mapKey}' does not exist.");
        int width = Math.Max(1, readInt(map["width"], 13));
        int height = Math.Max(1, readInt(map["height"], 13));
        List<PluginMapLayerSnapshot> layerSnapshots = [];
        if (map["layers"] is JsonObject layers)
        {
            foreach (KeyValuePair<string, JsonNode?> entry in layers)
            {
                if (entry.Value is not JsonObject layer)
                    continue;
                string tilesetKey = readString(layer["layerTileset"]);
                layerSnapshots.Add(new PluginMapLayerSnapshot(
                    entry.Key,
                    tilesetKey,
                    readIntGrid(layer["tiles"] as JsonArray, width, height),
                    readStringGrid(layer["autoTiles"] as JsonArray, width, height))
                {
                    Actors = readActors(map, entry.Key, width, height),
                });
            }
        }
        return new PluginMapSnapshot(
            mapKey,
            gameData.Maps.getMapDisplayName(mapKey),
            width,
            height,
            createRevision(map),
            layerSnapshots.AsReadOnly());
    }

    private IReadOnlyList<PluginMapActorSnapshot> readActors(
        JsonObject map,
        string layerName,
        int mapWidth,
        int mapHeight)
    {
        List<PluginMapActorSnapshot> snapshots = [];
        if (map["actors"]?[layerName] is not JsonArray actors)
            return snapshots.AsReadOnly();
        foreach (JsonNode? node in actors)
        {
            if (node is not JsonObject actor
                || !tryReadActorPosition(actor, mapWidth, mapHeight, out int x, out int y))
            {
                continue;
            }
            ActorVisualDescriptor? visual = previewService.tryResolveMapActorVisual(map, actor);
            string assetPath = visual is not null && GameAssetPath.IsCanonical(visual.TexturePath)
                ? visual.TexturePath
                : string.Empty;
            PluginMapActorRectSnapshot? sourceRect = visual is null
                ? null
                : new PluginMapActorRectSnapshot(
                    visual.BaseTextureRect.X,
                    visual.BaseTextureRect.Y,
                    visual.BaseTextureRect.Width,
                    visual.BaseTextureRect.Height);
            snapshots.Add(new PluginMapActorSnapshot(
                x,
                y,
                assetPath,
                sourceRect,
                visual?.Translation.X ?? 0,
                visual?.Translation.Y ?? 0,
                visual?.Scale.X ?? 1,
                visual?.Scale.Y ?? 1,
                visual?.Origin.X ?? 0,
                visual?.Origin.Y ?? 0,
                visual?.Rotation ?? 0,
                visual?.Material ?? string.Empty,
                visual?.MaterialParameters.DeepClone() as JsonObject ?? new JsonObject(),
                visual?.MapPreviewOpacity ?? 1));
        }
        return snapshots.AsReadOnly();
    }

    private static bool tryReadActorPosition(
        JsonObject actor,
        int mapWidth,
        int mapHeight,
        out int x,
        out int y)
    {
        x = 0;
        y = 0;
        if (actor["position"] is not JsonArray position
            || position.Count < 2
            || !tryReadInt(position[0], out x)
            || !tryReadInt(position[1], out y))
        {
            return false;
        }
        return x >= 0 && y >= 0 && x < mapWidth && y < mapHeight;
    }

    private PluginTilesetSnapshot readTileset(string tilesetKey)
    {
        if (!SnapshotJson.ToDictionary(gameData.Assets.TilesetData).TryGetValue(tilesetKey, out JsonObject? tileset))
            throw new KeyNotFoundException($"Tileset '{tilesetKey}' does not exist.");
        string assetPath = getTilesetAssetPath(tileset);
        JsonArray? sourcePassable = tileset["passable"] as JsonArray;
        List<bool> passable = [];
        int tileSize = Math.Max(1, gameData.Configs.getCellSize());
        int tileCount = getTileCount(
            assetPath,
            tileSize,
            sourcePassable?.Count ?? 0);
        for (int index = 0; index < tileCount; index += 1)
        {
            JsonNode? value = sourcePassable is not null && index < sourcePassable.Count
                ? sourcePassable[index]
                : null;
            passable.Add(value is JsonValue scalar
                && scalar.TryGetValue(out bool parsed)
                && parsed);
        }
        return new PluginTilesetSnapshot(
            tilesetKey,
            assetPath,
            tileSize,
            tileSize,
            tileCount,
            passable.AsReadOnly());
    }

    private PluginMapWriteResult replaceLayerAndSave(
        PluginMapLayerWriteRequest request,
        CancellationToken cancellationToken)
    {
        if (!SnapshotJson.ToDictionary(gameData.Maps.MapData).TryGetValue(request.MapKey, out JsonObject? map))
            return PluginMapWriteResult.Failed("The selected map no longer exists.", string.Empty);
        string currentRevision = createRevision(map);
        if (!string.Equals(request.BaseRevision, currentRevision, StringComparison.Ordinal))
            return PluginMapWriteResult.Conflicted(currentRevision);
        int width = readInt(map["width"], 0);
        int height = readInt(map["height"], 0);
        if (width <= 0 || height <= 0)
            return PluginMapWriteResult.Failed("The selected map has invalid dimensions.", currentRevision);
        if (map["layers"]?[request.LayerName] is not JsonObject layer)
            return PluginMapWriteResult.Failed("The selected map layer no longer exists.", currentRevision);
        if (!canWriteLayer(request.MapKey, request.LayerName))
            return PluginMapWriteResult.Failed("The selected map layer is hidden.", currentRevision);
        string tilesetKey = readString(layer["layerTileset"]);
        if (!string.Equals(
            request.ExpectedTilesetKey,
            tilesetKey,
            StringComparison.Ordinal))
        {
            return PluginMapWriteResult.Conflicted(currentRevision);
        }
        if (!SnapshotJson.ToDictionary(gameData.Assets.TilesetData).TryGetValue(tilesetKey, out JsonObject? tileset))
            return PluginMapWriteResult.Failed("The layer tileset no longer exists.", currentRevision);
        string assetPath = getTilesetAssetPath(tileset);
        int tileCount = getTileCount(
            assetPath,
            Math.Max(1, gameData.Configs.getCellSize()),
            (tileset["passable"] as JsonArray)?.Count ?? 0);
        string? tilesError = validateTiles(request.Tiles, width, height, tileCount);
        if (tilesError is not null)
            return PluginMapWriteResult.Failed(tilesError, currentRevision);
        string? autoTilesError = validateAutoTiles(request.AutoTiles, width, height);
        if (autoTilesError is not null)
            return PluginMapWriteResult.Failed(autoTilesError, currentRevision);

        cancellationToken.ThrowIfCancellationRequested();
        JsonArray tilesGrid = createTilesGrid(request.Tiles);
        JsonArray autoTilesGrid = createAutoTilesGrid(request.AutoTiles);
        SaveResult saveResult = gameData.ReplaceMapLayerAndSave(
            request.MapKey,
            request.LayerName,
            tilesGrid,
            autoTilesGrid);
        if (!saveResult.Success)
            return PluginMapWriteResult.Failed(saveResult.Details, currentRevision);

        JsonObject savedMap = SnapshotJson.ToDictionary(gameData.Maps.MapData)[request.MapKey];
        string savedRevision = createRevision(savedMap);
        refreshMap(request.MapKey, request.LayerName);
        return PluginMapWriteResult.Completed(savedRevision);
    }

    private string? validateTiles(
        IReadOnlyList<IReadOnlyList<int?>> tiles,
        int width,
        int height,
        int tileCount)
    {
        if (tiles is null || tiles.Count != height)
            return $"Tiles must contain exactly {height} rows.";
        for (int y = 0; y < height; y += 1)
        {
            IReadOnlyList<int?>? row = tiles[y];
            if (row is null || row.Count != width)
                return $"Tile row {y} must contain exactly {width} cells.";
            for (int x = 0; x < width; x += 1)
            {
                int? value = row[x];
                if (value is not null && (value.Value < 0 || value.Value >= tileCount))
                    return $"Tile at ({x}, {y}) is outside the tileset range.";
            }
        }
        return null;
    }

    private string? validateAutoTiles(
        IReadOnlyList<IReadOnlyList<string?>> autoTiles,
        int width,
        int height)
    {
        if (autoTiles is null || autoTiles.Count != height)
            return $"AutoTiles must contain exactly {height} rows.";
        for (int y = 0; y < height; y += 1)
        {
            IReadOnlyList<string?>? row = autoTiles[y];
            if (row is null || row.Count != width)
                return $"AutoTile row {y} must contain exactly {width} cells.";
            for (int x = 0; x < width; x += 1)
            {
                string? value = row[x];
                if (value is not null && !gameData.Assets.AutoTileData.ContainsKey(value))
                    return $"AutoTile at ({x}, {y}) does not exist.";
            }
        }
        return null;
    }

    private static IReadOnlyList<IReadOnlyList<int?>> readIntGrid(
        JsonArray? source,
        int width,
        int height)
    {
        List<IReadOnlyList<int?>> result = [];
        for (int y = 0; y < height; y += 1)
        {
            JsonArray? sourceRow = source is not null && y < source.Count
                ? source[y] as JsonArray
                : null;
            int?[] row = new int?[width];
            for (int x = 0; x < width; x += 1)
            {
                if (sourceRow is not null
                    && x < sourceRow.Count
                    && sourceRow[x] is JsonValue scalar
                    && scalar.TryGetValue(out int value))
                {
                    row[x] = value;
                }
            }
            result.Add(Array.AsReadOnly(row));
        }
        return result.AsReadOnly();
    }

    private static IReadOnlyList<IReadOnlyList<string?>> readStringGrid(
        JsonArray? source,
        int width,
        int height)
    {
        List<IReadOnlyList<string?>> result = [];
        for (int y = 0; y < height; y += 1)
        {
            JsonArray? sourceRow = source is not null && y < source.Count
                ? source[y] as JsonArray
                : null;
            string?[] row = new string?[width];
            for (int x = 0; x < width; x += 1)
            {
                if (sourceRow is not null
                    && x < sourceRow.Count
                    && sourceRow[x] is JsonValue scalar
                    && scalar.TryGetValue(out string? value))
                {
                    row[x] = value;
                }
            }
            result.Add(Array.AsReadOnly(row));
        }
        return result.AsReadOnly();
    }

    private static JsonArray createTilesGrid(
        IReadOnlyList<IReadOnlyList<int?>> source)
    {
        JsonArray result = new JsonArray();
        foreach (IReadOnlyList<int?> sourceRow in source)
        {
            JsonArray row = new JsonArray();
            foreach (int? value in sourceRow)
                row.Add(value);
            result.Add(row);
        }
        return result;
    }

    private static JsonArray createAutoTilesGrid(
        IReadOnlyList<IReadOnlyList<string?>> source)
    {
        JsonArray result = new JsonArray();
        foreach (IReadOnlyList<string?> sourceRow in source)
        {
            JsonArray row = new JsonArray();
            foreach (string? value in sourceRow)
                row.Add(value);
            result.Add(row);
        }
        return result;
    }

    private static int readInt(JsonNode? value, int fallback)
    {
        return tryReadInt(value, out int parsed) ? parsed : fallback;
    }

    private static bool tryReadInt(JsonNode? value, out int parsed)
    {
        return int.TryParse(
            value?.ToString(),
            NumberStyles.Integer,
            CultureInfo.InvariantCulture,
            out parsed);
    }

    private static string readString(JsonNode? value)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out string? parsed)
                ? parsed ?? string.Empty
                : string.Empty;
    }

    private static string getTilesetAssetPath(JsonObject tileset)
    {
        string assetPath = readString(tileset["fileName"]);
        return GameAssetPath.IsCanonical(assetPath)
            ? assetPath
            : string.Empty;
    }

    private int getTileCount(
        string assetPath,
        int tileSize,
        int fallback)
    {
        string filePath = ResolveAssetFile(assetPath);
        if (filePath.Length == 0)
            return fallback;
        try
        {
            using Bitmap image = new Bitmap(filePath);
            return image.PixelSize.Width / tileSize
                * (image.PixelSize.Height / tileSize);
        }
        catch (Exception)
        {
            return fallback;
        }
    }

    private static string createRevision(JsonObject map)
    {
        byte[] payload = Encoding.UTF8.GetBytes(map.ToJsonString());
        return Convert.ToHexString(SHA256.HashData(payload));
    }

    private static T runOnUiThread<T>(Func<T> action)
    {
        return Dispatcher.UIThread.CheckAccess()
            ? action()
            : Dispatcher.UIThread.InvokeAsync(action).GetAwaiter().GetResult();
    }
}
