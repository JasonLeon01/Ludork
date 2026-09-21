using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class UiAnimationSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string? Name => ReadString("name");
    public bool HasTarget => ReadValue("target") is not null;
    public string? Target => ReadString("target");
    public double? Duration => ReadNumber("duration");
    public JsonArray Pivot => ReadArray("pivot");
    public JsonObject Tracks => ReadObject("tracks");
}
