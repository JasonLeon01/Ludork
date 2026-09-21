using Ludork.Models;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public int? AddMapActor(string mapKey, string layerName, string blueprint, int x, int y)
        => AddMapActor(mapKey, layerName, new JsonObject { ["bp"] = blueprint }, x, y);

    public int? AddMapActor(string mapKey, string layerName, MapActorSnapshot actor, int x, int y, JsonObject? overrides)
        => AddMapActor(mapKey, layerName, actor.ToJson(), x, y, overrides);

    public bool UpdateMapLight(string mapKey, int index, MapLightSnapshot expected, MapPoint? position = null, double? radius = null)
    {
        JsonObject next = expected.ToJson();
        if (position is MapPoint point)
            next["position"] = new JsonArray(point.X, point.Y);
        if (radius is double value)
            next["radius"] = value;
        return UpdateMapLight(mapKey, index, expected.ToJson(), next);
    }

    public bool DeleteMapLight(string mapKey, int index, MapLightSnapshot expected)
        => DeleteMapLight(mapKey, index, expected.ToJson());
}
