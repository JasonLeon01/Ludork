using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class ConfigSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public IReadOnlyDictionary<string, ConfigFieldSnapshot> Fields => SnapshotData.Where(pair => pair.Value is JsonObject)
        .ToDictionary(pair => pair.Key, pair => new ConfigFieldSnapshot((JsonObject)pair.Value!));
}
