using Ludork.Models;
using Ludork.Services;
using System.Text.Json.Nodes;
using System;

namespace Ludork.Views.Utils;

public sealed class CommonFunctionEditorDocument : IDisposable
{
    private readonly ProjectDataStore gameData;
    private JsonObject data = [];
    private readonly EditorDocument? resourceDocument;
    private bool committing;

    private CommonFunctionEditorDocument(ProjectDataStore gameData, string name)
    {
        this.gameData = gameData;
        resourceDocument = gameData.GetDocument("CommonFunctions", name);
        if (resourceDocument is not null)
            resourceDocument.Changed += onResourceChanged;
        Reload();
    }

    public string Name => resourceDocument?.Key ?? string.Empty;
    public EditorDocument? ResourceDocument => resourceDocument;
    public event EventHandler? ExternalChanged;
    public JsonObject Data => data;

    public static CommonFunctionEditorDocument? Create(
        ProjectDataStore gameData,
        string name)
    {
        return gameData.Blueprints.CommonFunctionsData.ContainsKey(name)
            ? new CommonFunctionEditorDocument(gameData, name)
            : null;
    }

    public bool Reload()
    {
        if (!gameData.Blueprints.CommonFunctionsData.TryGetValue(Name, out CommonFunctionSnapshot? stored))
        {
            data = [];
            return false;
        }
        data = stored.ToJson();
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
        data = resourceDocument?.Data ?? data;
        ensureObject(data, "nodeGraph")["common"] = result.EventGraph.DeepClone();
        ensureObject(data, "startNodes")["common"] = result.StartNode?.DeepClone();
        committing = true;
        try
        {
            return gameData.Blueprints.UpdateCommonFunction(Name, data);
        }
        finally
        {
            committing = false;
        }
    }

    public void Dispose()
    {
        if (resourceDocument is not null)
            resourceDocument.Changed -= onResourceChanged;
    }

    private void onResourceChanged(object? sender, EventArgs args)
    {
        if (committing || JsonNode.DeepEquals(data, resourceDocument?.Data))
            return;
        Reload();
        ExternalChanged?.Invoke(this, EventArgs.Empty);
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
