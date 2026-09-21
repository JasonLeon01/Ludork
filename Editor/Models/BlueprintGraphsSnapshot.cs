using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphsSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public IReadOnlyDictionary<string, BlueprintEventGraphSnapshot> Events => ReadObjects("nodeGraph", value => new BlueprintEventGraphSnapshot(value));
    public JsonObject StartNodes => ReadObject("startNodes");
}
