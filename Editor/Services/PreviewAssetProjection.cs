using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class PreviewAssetProjection(ProjectAssetPaths assetPaths)
{
    private readonly object sync = new();
    private readonly Dictionary<string, string[]> materialTextureParameters = new();
    private IReadOnlyDictionary<string, string> mapping = new Dictionary<string, string>();

    public JsonObject Project(JsonObject request)
    {
        lock (sync)
        {
            mapping = assetPaths.CapturePreviewMapping();
            if (request["materials"] is JsonObject materials)
            {
                materialTextureParameters.Clear();
                foreach (KeyValuePair<string, JsonNode?> entry in materials)
                    if (entry.Value is JsonObject material)
                        materialTextureParameters[entry.Key] = textureParameters(material);
            }
            return mapping.Count == 0 ? request : project(request);
        }
    }

    private JsonObject project(JsonObject request)
    {
        JsonObject result = (JsonObject)request.DeepClone();
        projectMaterials(result["materials"] as JsonObject);
        switch (JsonScalar.String(result["type"]))
        {
            case "render":
            case "resolveReparent":
                projectUi(result["asset"] as JsonObject);
                if (result["dependencies"] is JsonObject dependencies)
                    foreach (JsonObject dependency in dependencies.Select(pair => pair.Value).OfType<JsonObject>())
                        projectUi(dependency);
                break;
            case "renderParticle":
                projectParticle(result["asset"] as JsonObject);
                break;
            case "renderSubtitle":
                projectValue(result, "videoPath");
                break;
            case "renderMaterial":
                projectValue(result, "texturePath");
                projectMaterial(result["asset"] as JsonObject);
                projectParameters(result["parameters"] as JsonObject, result["asset"] as JsonObject);
                break;
            case "renderActorBatch":
                projectSprites(result["items"] as JsonArray);
                break;
            case "renderMap":
                projectScene(result["scene"] as JsonObject);
                if (result["sceneChanges"] is JsonObject changes)
                {
                    projectScene(changes["fields"] as JsonObject);
                    projectRegions(changes["regions"] as JsonArray);
                    projectSprites(changes["sprites"]?["upsert"] as JsonArray);
                }
                break;
        }
        return result;
    }

    private void projectUi(JsonObject? asset)
    {
        if (asset?["root"] is JsonObject root)
            projectUiNode(root);
    }

    private void projectUiNode(JsonObject node)
    {
        if (node["properties"] is JsonObject properties)
        {
            foreach (string name in new[] { "texture", "texturePath", "backgroundTexture", "fillTexture",
                "windowSkin", "lineTexture", "handleTexture", "font" })
                projectValue(properties, name);
            projectInstanceParameters(properties);
        }
        if (node["children"] is JsonArray children)
            foreach (JsonObject child in children.OfType<JsonObject>())
                projectUiNode(child);
    }

    private void projectScene(JsonObject? scene)
    {
        if (scene is null)
            return;
        projectValue(scene, "fog");
        projectValue(scene, "panorama");
        foreach (string name in new[] { "tilesets", "autoTiles" })
            if (scene[name] is JsonObject tiles)
                foreach (JsonObject tile in tiles.Select(pair => pair.Value).OfType<JsonObject>())
                    projectValue(tile, "fileName");
        if (scene["particles"] is JsonObject particles)
            foreach (JsonObject particle in particles.Select(pair => pair.Value).OfType<JsonObject>())
                projectParticle(particle);
        projectRegions(scene["regions"] as JsonArray);
        projectSprites(scene["sprites"] as JsonArray);
    }

    private void projectRegions(JsonArray? regions)
    {
        if (regions is null)
            return;
        foreach (JsonObject region in regions.OfType<JsonObject>())
            if (region["map"] is JsonObject map)
                projectValue(map, "fog");
    }

    private void projectSprites(JsonArray? sprites)
    {
        if (sprites is null)
            return;
        foreach (JsonObject sprite in sprites.OfType<JsonObject>())
        {
            projectValue(sprite, "texturePath");
            projectInstanceParameters(sprite);
        }
    }

    private void projectParticle(JsonObject? particle)
    {
        if (particle?["tracks"] is JsonArray tracks)
            foreach (JsonObject track in tracks.OfType<JsonObject>())
                projectValue(track, "texture");
    }

    private void projectMaterials(JsonObject? materials)
    {
        if (materials is not null)
            foreach (JsonObject material in materials.Select(pair => pair.Value).OfType<JsonObject>())
                projectMaterial(material);
    }

    private void projectMaterial(JsonObject? material)
    {
        if (material?["parameters"] is JsonArray parameters)
            foreach (JsonObject parameter in parameters.OfType<JsonObject>())
                if (JsonScalar.String(parameter["type"]) == "texture")
                    projectValue(parameter, "default");
        if (material?["graph"]?["nodes"] is JsonArray nodes)
            foreach (JsonObject node in nodes.OfType<JsonObject>())
                if (JsonScalar.String(node["type"]) == "SampleTexture" && node["inputs"] is JsonObject inputs)
                    projectValue(inputs, "Texture");
    }

    private void projectParameters(JsonObject? values, JsonObject? material)
    {
        if (values is not null && material is not null)
            foreach (string key in textureParameters(material))
                projectValue(values, key);
    }

    private void projectInstanceParameters(JsonObject owner)
    {
        if (JsonScalar.String(owner["material"]) is string material
            && owner["materialParameters"] is JsonObject values
            && materialTextureParameters.TryGetValue(material, out string[]? names))
            foreach (string name in names)
                projectValue(values, name);
    }

    private static string[] textureParameters(JsonObject material)
    {
        return (material["parameters"] as JsonArray ?? []).OfType<JsonObject>()
            .Where(parameter => JsonScalar.String(parameter["type"]) == "texture")
            .Select(parameter => JsonScalar.String(parameter["id"])).OfType<string>().ToArray();
    }

    private void projectValue(JsonObject owner, string name)
    {
        if (JsonScalar.String(owner[name]) is string value && mapping.TryGetValue(value, out string? physical))
            owner[name] = physical;
    }
}
