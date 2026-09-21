using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class TextStyleSnapshot(JsonObject data) : EditorDataSnapshot(data)
{
    public double CharacterSize => Number("characterSize", 22);
    public double SlantAngle => Number("slantAngle");
    public double LetterSpacing => Number("letterSpacing", 1);
    public double LineSpacing => Number("lineSpacing", 1);
    public MapColour FillColour => MapSnapshotValues.Colour(SnapshotData["fillColor"], new(255,255,255,255));
    public TextStyleFlags? Flags => SnapshotData["style"] is JsonObject value ? new(
        value["bold"]?.GetValue<bool?>() ?? false, value["italic"]?.GetValue<bool?>() ?? false,
        value["underlined"]?.GetValue<bool?>() ?? false, value["strikeThrough"]?.GetValue<bool?>() ?? false) : null;
    public TextOutline? Outline => SnapshotData["outline"] is JsonObject value
        ? new(MapSnapshotValues.Colour(value["color"], new(0,0,0,255)), MapSnapshotValues.Number(value["thickness"])) : null;
}
