using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class UiNodeSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string? Id => ReadString("id");
    public string? Name => ReadString("name");
    public string? ControlId => ReadString("controlId");
    public JsonObject Properties => ReadObject("properties");
    public JsonObject Slot => ReadObject("slot");
    public JsonObject Editor => ReadObject("editor");
    public IReadOnlyList<UiNodeSnapshot> Children => ReadArray("children").OfType<JsonObject>().Select(value => new UiNodeSnapshot(value)).ToArray();
}
