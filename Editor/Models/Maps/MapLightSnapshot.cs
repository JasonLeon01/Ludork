using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class MapLightSnapshot
{
    internal MapLightSnapshot(JsonObject data) => Data = data;
    internal JsonObject Data { get; }
    public bool HasPosition => Data["position"] is JsonArray { Count: >= 2 };
    public MapPoint Position => MapSnapshotValues.Point(Data["position"]);
    public MapColour Colour => MapSnapshotValues.Colour(Data["color"], new(255, 255, 255, 255));
    public double Radius => MapSnapshotValues.Number(Data["radius"]);
    public double Intensity => MapSnapshotValues.Number(Data["intensity"], 1);
    public JsonObject ToJson() => (JsonObject)Data.DeepClone();
}
