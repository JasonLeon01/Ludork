using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class GameDataService
{
    public JsonObject? ReadMapSnapshot(string key)
    {
        return getMap(key)?.DeepClone() as JsonObject;
    }

    public JsonObject? ReadWorldMapSnapshot(string key)
    {
        return getWorldMap(key)?.DeepClone() as JsonObject;
    }
}
