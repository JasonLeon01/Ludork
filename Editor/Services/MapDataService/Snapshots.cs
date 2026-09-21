using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed partial class MapDataService
{
    public MapDocumentSnapshot? ReadMapDocument(string key)
    {
        return getMap(key) is JsonObject data ? MapDocumentCodec.Decode(data) : null;
    }

    public JsonObject? ReadMapSnapshot(string key)
    {
        return getMap(key)?.DeepClone() as JsonObject;
    }

}
