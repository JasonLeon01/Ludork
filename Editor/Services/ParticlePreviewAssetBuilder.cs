using Ludork.Models;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal static class ParticlePreviewAssetBuilder
{
    public static JsonObject Build(JsonObject source, IReadOnlyDictionary<string, CurveSnapshot> curves)
    {
        JsonObject asset = (JsonObject)source.DeepClone();
        asset["type"] = "particle";
        foreach (JsonObject track in (asset["tracks"] as JsonArray ?? []).OfType<JsonObject>())
        {
            if (track["curves"] is not JsonObject channels)
                continue;
            foreach (string channel in channels.Select(pair => pair.Key).ToArray())
            {
                if (channels[channel] is not JsonValue value || !value.TryGetValue<string>(out string? key))
                    continue;
                if (!curves.TryGetValue(key, out CurveSnapshot? curve))
                    return new JsonObject { ["previewError"] = "Particle curve was not found: " + key };
                channels[channel] = curve.ToJson();
            }
        }
        return asset;
    }
}
