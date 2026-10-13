using System.Text.Json.Nodes;

namespace Ludork.Models;

public static class MapFogSettings
{
    public static JsonObject CreateDefault() => new()
    {
        ["enabled"] = false, ["colour"] = new JsonArray(255, 255, 255, 255),
        ["density"] = 0.5, ["scale"] = 256, ["velocity"] = new JsonArray(15, 15),
        ["turbulence"] = 0.5, ["seed"] = 0,
    };
}
