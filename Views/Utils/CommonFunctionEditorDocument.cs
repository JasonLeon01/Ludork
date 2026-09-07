using Ludork.Models;
using Ludork.Services;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

public sealed class CommonFunctionEditorDocument
{
    private readonly GameDataService gameData;
    private JsonObject data = [];

    private CommonFunctionEditorDocument(GameDataService gameData, string name)
    {
        this.gameData = gameData;
        Name = name;
        Reload();
    }

    public string Name { get; }
    public JsonObject Data => data;

    public static CommonFunctionEditorDocument? Create(
        GameDataService gameData,
        string name)
    {
        return gameData.CommonFunctionsData.ContainsKey(name)
            ? new CommonFunctionEditorDocument(gameData, name)
            : null;
    }

    public bool Reload()
    {
        if (!gameData.CommonFunctionsData.TryGetValue(Name, out JsonObject? stored))
        {
            data = [];
            return false;
        }
        data = (JsonObject)stored.DeepClone();
        return true;
    }

    public JsonObject GetEventGraph()
    {
        return data["nodeGraph"] is JsonObject nodeGraph && nodeGraph["common"] is JsonObject eventGraph
            ? eventGraph
            : new JsonObject { ["nodes"] = new JsonArray(), ["links"] = new JsonArray() };
    }

    public JsonObject GetStartNodes()
    {
        return data["startNodes"] as JsonObject ?? [];
    }

    public bool CommitGraph(BlueprintGraphSaveResult result)
    {
        if (JsonNode.DeepEquals(GetEventGraph(), result.EventGraph)
            && JsonNode.DeepEquals(GetStartNodes()["common"], result.StartNode))
        {
            return false;
        }
        ensureObject(data, "nodeGraph")["common"] = result.EventGraph.DeepClone();
        ensureObject(data, "startNodes")["common"] = result.StartNode?.DeepClone();
        return gameData.UpdateCommonFunction(Name, data);
    }

    private static JsonObject ensureObject(JsonObject parent, string name)
    {
        if (parent[name] is JsonObject value)
            return value;
        value = [];
        parent[name] = value;
        return value;
    }
}
