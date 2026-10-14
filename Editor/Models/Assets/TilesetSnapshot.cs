using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class TilesetSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string? Name => SnapshotData["name"]?.GetValue<string>();
    public string FileName => Text("fileName");
    public bool? AutoTilePassable => SnapshotData["passable"] is JsonValue value && value.TryGetValue(out bool passable) ? passable : null;
    public IReadOnlyList<bool?> Passable => (SnapshotData["passable"] as JsonArray ?? []).Select(value => value?.GetValue<bool?>()).ToArray();
    public string Material => Text("material");
    public IReadOnlyDictionary<string, string> Materials => (SnapshotData["materials"] as JsonObject ?? []).ToDictionary(pair => pair.Key, pair => pair.Value!.GetValue<string>());
}
