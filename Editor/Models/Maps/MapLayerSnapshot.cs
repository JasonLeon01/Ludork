using System.Text.Json.Nodes;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace Ludork.Models;

public sealed class MapLayerSnapshot
{
    internal MapLayerSnapshot(JsonObject data) => Data = data;
    internal JsonObject Data { get; }
    public string Name => MapSnapshotValues.Text(Data["layerName"]);
    public string Tileset => MapSnapshotValues.Text(Data["layerTileset"]);
    public string ShaderPath => MapSnapshotValues.Text(Data["shaderPath"]);
    public bool Visible => Data["visible"]?.GetValue<bool?>() ?? true;
    public int? TileAt(int x, int y) => int.TryParse(MapSnapshotValues.Cell(Data["tiles"], x, y)?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out int value) ? value : null;
    public string? AutoTileAt(int x, int y) => MapSnapshotValues.Cell(Data["autoTiles"], x, y) is JsonNode value ? MapSnapshotValues.Text(value) : null;
    public bool CellMatches(int firstX, int firstY, int secondX, int secondY)
        => JsonNode.DeepEquals(MapSnapshotValues.Cell(Data["tiles"], firstX, firstY), MapSnapshotValues.Cell(Data["tiles"], secondX, secondY))
            && JsonNode.DeepEquals(MapSnapshotValues.Cell(Data["autoTiles"], firstX, firstY), MapSnapshotValues.Cell(Data["autoTiles"], secondX, secondY));
    public IEnumerable<string> AutoTileKeys => (Data["autoTiles"] as JsonArray ?? [])
        .OfType<JsonArray>().SelectMany(row => row).Select(value => value?.GetValue<string>())
        .Where(value => !string.IsNullOrWhiteSpace(value)).Select(value => value!).Distinct();
    internal JsonArray? AutoTileGrid => Data["autoTiles"] as JsonArray;
    public JsonObject ToJson() => (JsonObject)Data.DeepClone();
}
