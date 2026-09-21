using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintLinkSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string LinkType => ReadString("linkType") ?? string.Empty;
    public JsonNode? Left => ReadValue("left");
    public JsonNode? Right => ReadValue("right");
    public int? LeftOutputPin => ReadInteger("leftOutPin");
    public int? RightInputPin => ReadInteger("rightInPin");
}
