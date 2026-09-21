using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class WorldDataService
{
    public JsonObject? ReadWorldMapSnapshot(string key)
    {
        return getWorldMap(key)?.DeepClone() as JsonObject;
    }

}
