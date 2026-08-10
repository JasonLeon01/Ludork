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
        data = normalize(stored);
        return true;
    }

    public JsonObject GetEventGraph()
    {
        JsonObject nodeGraph = ensureObject(data, "nodeGraph");
        JsonObject eventGraph = ensureObject(nodeGraph, "common");
        ensureArray(eventGraph, "nodes");
        ensureArray(eventGraph, "links");
        return eventGraph;
    }

    public JsonObject GetStartNodes()
    {
        return ensureObject(data, "startNodes");
    }

    public bool CommitGraph()
    {
        data = normalize(data);
        return gameData.UpdateCommonFunction(Name, data);
    }

    private static JsonObject normalize(JsonObject source)
    {
        JsonObject result = (JsonObject)source.DeepClone();
        if (!result.ContainsKey("parent"))
            result["parent"] = null;
        JsonObject nodeGraph = ensureObject(result, "nodeGraph");
        JsonObject eventGraph = ensureObject(nodeGraph, "common");
        ensureArray(eventGraph, "nodes");
        ensureArray(eventGraph, "links");
        ensureObject(result, "startNodes");
        return result;
    }

    private static JsonObject ensureObject(JsonObject parent, string name)
    {
        if (parent[name] is JsonObject value)
            return value;
        value = [];
        parent[name] = value;
        return value;
    }

    private static JsonArray ensureArray(JsonObject parent, string name)
    {
        if (parent[name] is JsonArray value)
            return value;
        value = [];
        parent[name] = value;
        return value;
    }
}
