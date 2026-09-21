using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintNodeSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public string NodeFunction => ReadString("nodeFunction") ?? string.Empty;
    public JsonArray Parameters => ReadArray("params");
    public double X => coordinate(0);
    public double Y => coordinate(1);

    private double coordinate(int index)
    {
        return ReadFiniteNumber(ReadArray("pos").ElementAtOrDefault(index)) ?? 0;
    }
}
