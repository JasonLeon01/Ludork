using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class TextConfigSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public string Type => Text("type");
    public string Name => Text("name");
    public string Font => Text("font");
    public string LineAlignment => Text("lineAlignment", "default");
    public TextStyleSnapshot PlainStyle => new(SnapshotData);
    public TextStyleSnapshot? DefaultStyle => SnapshotData["defaultStyle"] is JsonObject value ? new(value) : null;
    public IReadOnlyList<string> StyleOrder => Strings("styleOrder");
    public IReadOnlyDictionary<string, TextStyleSnapshot> Styles => (SnapshotData["styles"] as JsonObject ?? []).Where(pair => pair.Value is JsonObject)
        .ToDictionary(pair => pair.Key, pair => new TextStyleSnapshot((JsonObject)pair.Value!));
    public TextGlow? Glow => SnapshotData["glow"] is JsonObject value ? TextStyleValues.ReadGlow(value) : null;
    public TextGradient? Gradient => SnapshotData["gradient"] is JsonObject value ? TextStyleValues.ReadGradient(value) : null;
}
