using System.Text.Json.Nodes;
using System.Globalization;

namespace Ludork.Models;

public sealed class MapActorSnapshot
{
    internal MapActorSnapshot(JsonObject data) => Data = data;
    internal JsonObject Data { get; }
    public string Tag => MapSnapshotValues.Text(Data["tag"]);
    public string Blueprint => MapSnapshotValues.Text(Data["bp"]);
    public string Type => MapSnapshotValues.Text(Data["type"]);
    public MapPoint Position => MapSnapshotValues.Point(Data["position"]);
    public string? RuntimeId => Data["runtimeId"]?.GetValue<string>();
    public string? ParentRuntimeId => Data["parentRuntimeId"]?.GetValue<string>();
    public JsonObject? ReadVisual() => Data["visual"]?.DeepClone() as JsonObject;
    public bool TryGetGridPosition(out int x, out int y)
    {
        x = 0;
        y = 0;
        return Data["position"] is JsonArray { Count: >= 2 } position
            && int.TryParse(position[0]?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out x)
            && int.TryParse(position[1]?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out y);
    }
    public JsonObject ToJson() => (JsonObject)Data.DeepClone();
}
