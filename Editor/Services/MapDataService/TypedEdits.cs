using Ludork.Models;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public int? AddMapActor(string mapKey, string layerName, string blueprint, int x, int y)
        => AddMapActor(mapKey, layerName, new JsonObject { ["bp"] = blueprint }, x, y);

    public int? AddMapActor(string mapKey, string layerName, MapActorSnapshot actor, int x, int y, JsonObject? overrides)
        => AddMapActor(mapKey, layerName, actor.ToJson(), x, y, overrides);

}
