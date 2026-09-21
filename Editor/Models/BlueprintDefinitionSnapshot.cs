using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintDefinitionSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string? Type => ReadString("type");
    public string Parent => ReadString("parent") ?? string.Empty;
    public JsonObject Attributes => ReadObject("attrs");
    public BlueprintGraphsSnapshot Graph => new(ReadObject("graph"));
}
