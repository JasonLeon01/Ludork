using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintEventGraphSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public IReadOnlyList<BlueprintNodeSnapshot> Nodes => ReadArray("nodes").OfType<JsonObject>().Select(value => new BlueprintNodeSnapshot(value)).ToArray();
    public IReadOnlyList<BlueprintLinkSnapshot> Links => ReadArray("links").OfType<JsonObject>().Select(value => new BlueprintLinkSnapshot(value)).ToArray();
    public JsonArray GetNodeJson() => ReadArray("nodes");
}
