using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class ConfigFieldSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Type => Text("type");
    public JsonNode? CurrentValue => ReadValue("value");
    public bool HasValue => SnapshotData.ContainsKey("value");
    public string AssetBase => Text("base");
    public JsonNode? Extensions => ReadValue("ext");
}
