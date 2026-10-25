using Ludork.Models;
using Ludork.Plugin.Abstractions;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services.Plugins;

internal sealed partial class MapEditorHostBridge
{
    private IReadOnlyList<IReadOnlyList<PluginMapPassage>> readPassage(JsonObject map, int width, int height)
    {
        JsonObject layers = map["layers"] as JsonObject ?? [];
        List<string> layerOrder = (map["layerOrder"] as JsonArray ?? [])
            .Select(readString).Where(layers.ContainsKey).Distinct(StringComparer.Ordinal).ToList();
        layerOrder.AddRange(layers.Select(entry => entry.Key).Where(name => !layerOrder.Contains(name, StringComparer.Ordinal)));
        layerOrder.Reverse();
        IReadOnlyDictionary<string, JsonObject> tilesets = SnapshotJson.ToDictionary(gameData.Assets.TilesetData);
        IReadOnlyDictionary<string, JsonObject> autoTiles = SnapshotJson.ToDictionary(gameData.Assets.AutoTileData);
        bool[,] passable = new bool[height, width];
        bool[,,] directions = new bool[height, width, 4];
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                passable[y, x] = true;
                for (int direction = 0; direction < 4; direction++)
                    directions[y, x, direction] = true;
                bool foundPassability = false;
                bool foundDirections = false;
                foreach (string layerName in layerOrder)
                {
                    if (layers[layerName] is not JsonObject layer || !readBool(layer["visible"], true))
                        continue;
                    string tilesetKey = readString(layer["layerTileset"]);
                    tilesets.TryGetValue(tilesetKey, out JsonObject? tileset);
                    JsonNode? tileValue = readCell(layer["tiles"], x, y);
                    bool hasTile = tryReadInt(tileValue, out int tile);
                    string autoTileKey = readString(readCell(layer["autoTiles"], x, y));
                    if (!foundPassability && (hasTile || autoTileKey.Length != 0))
                    {
                        passable[y, x] = autoTileKey.Length != 0
                            ? autoTiles.TryGetValue(autoTileKey, out JsonObject? autoTile) && readBool(autoTile["passable"], true)
                            : readBool(readIndex(tileset?["passable"], tile), false);
                        foundPassability = true;
                    }
                    if (!foundDirections && hasTile)
                    {
                        JsonArray? tileDirections = readIndex(tileset?["dir4"], tile) as JsonArray;
                        if (tileDirections is { Count: 4 })
                            for (int direction = 0; direction < 4; direction++)
                                directions[y, x, direction] = readBool(tileDirections[direction], true);
                        foundDirections = true;
                    }
                    if (foundPassability && foundDirections)
                        break;
                }
            }
        }
        List<IReadOnlyList<PluginMapPassage>> rows = [];
        for (int y = 0; y < height; y++)
        {
            PluginMapPassage[] row = new PluginMapPassage[width];
            for (int x = 0; x < width; x++)
            {
                row[x] = new PluginMapPassage(passable[y, x],
                    canMove(x, y, x, y - 1, 3, 0),
                    canMove(x, y, x + 1, y, 2, 1),
                    canMove(x, y, x, y + 1, 0, 3),
                    canMove(x, y, x - 1, y, 1, 2));
            }
            rows.Add(Array.AsReadOnly(row));
        }
        return rows.AsReadOnly();

        bool canMove(int x, int y, int targetX, int targetY, int direction, int opposite)
        {
            return targetX >= 0 && targetY >= 0 && targetX < width && targetY < height
                && passable[y, x] && passable[targetY, targetX]
                && directions[y, x, direction] && directions[targetY, targetX, opposite];
        }
    }

    private static JsonNode? readCell(JsonNode? grid, int x, int y) => readIndex(readIndex(grid, y), x);

    private static JsonNode? readIndex(JsonNode? node, int index)
        => node is JsonArray array && index >= 0 && index < array.Count ? array[index] : null;

    private static bool readBool(JsonNode? node, bool fallback)
        => node is JsonValue value && value.TryGetValue(out bool result) ? result : fallback;
}
