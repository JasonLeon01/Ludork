using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class CommonFunctionSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string? Parent => ReadString("parent");
    public string? Type => ReadString("type");
    public BlueprintGraphsSnapshot Graph => new(ToJson());
}
