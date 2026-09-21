using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class WorldMapSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Name => Text("worldName");
    public int Width => (int)Number("width");
    public int Height => (int)Number("height");
    public string Fog => Text("fog");
    public int FogPower => (int)Number("fogPower");
    public double FogOx => Number("fogOx");
    public double FogOy => Number("fogOy");
    public int FogDistort => (int)Number("fogDistort");
    public string Panorama => Text("panorama");
    public IReadOnlyList<string> LayerOrder => Strings("layerOrder");
    public IReadOnlyList<WorldMapPlacement> Placements => (SnapshotData["placements"] as JsonArray ?? []).OfType<JsonObject>()
        .Select(item => new WorldMapPlacement(item["map"]?.GetValue<string>() ?? string.Empty,
            new WorldMapRect(MapSnapshotValues.Integer(item["rect"]?[0]), MapSnapshotValues.Integer(item["rect"]?[1]),
                MapSnapshotValues.Integer(item["rect"]?[2]), MapSnapshotValues.Integer(item["rect"]?[3])))).ToArray();
}
