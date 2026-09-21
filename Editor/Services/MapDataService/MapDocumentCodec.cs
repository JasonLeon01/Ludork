using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

public static class MapDocumentCodec
{
    public static MapDocumentSnapshot Decode(JsonObject data) => new((JsonObject)data.DeepClone());
    public static void ApplyEdits(MapDocumentSnapshot document, MapDataEditedEventArgs edits)
    {
        edits.ApplyTo(document.Data);
        document.RefreshIndexes(edits);
    }
    public static JsonObject Encode(MapDocumentSnapshot document) => document.ToJson();
    public static MapActorSnapshot CreateActor(string blueprint) => new(new JsonObject { ["bp"] = blueprint });
    public static MapActorSnapshot DecodeActor(JsonObject actor) => new((JsonObject)actor.DeepClone());
    public static MapLightSnapshot DecodeLight(JsonObject light) => new((JsonObject)light.DeepClone());
}
