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
    public TileMaterialSnapshot? Material => SnapshotData["material"] is JsonObject value ? new(value) : null;
    public IReadOnlyList<TileMaterialSnapshot> Materials => Objects("materials", value => new TileMaterialSnapshot(value));
}
