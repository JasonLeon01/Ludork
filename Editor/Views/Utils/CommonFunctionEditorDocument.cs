using Ludork.Models;
using Ludork.Services;
using System.Text.Json.Nodes;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views.Utils;

public sealed class CommonFunctionEditorDocument : IDisposable
{
    private readonly ProjectDataStore gameData;
    private JsonObject data = [];
    private readonly EditorDocument? resourceDocument;
    private long sourceRevision;
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
    internal event EventHandler<BlueprintGraphParametersChangedEventArgs>? GraphParametersChanged;
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
        sourceRevision = resourceDocument?.Revision ?? 0;
        if (resourceDocument?.InternalData is not JsonObject stored)
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
            sourceRevision = resourceDocument?.Revision ?? 0;
        }
    }

    internal bool CommitGraph(BlueprintGraphDocument graph)
    {
        IReadOnlyList<JsonDataEdit>? changes = BlueprintGraphCodec.GetParameterEdits(graph);
        if (changes is null || data["nodeGraph"]?["common"] is not JsonObject)
        {
            bool changed = CommitGraph(BlueprintGraphCodec.Save(graph));
            graph.AcceptChanges();
            return changed;
        }
        object[] prefix = ["nodeGraph", "common"];
        JsonDataEdit[] edits = changes.Select(edit => edit.WithPath(prefix.Concat(edit.Path))).ToArray();
        committing = true;
        bool applied;
        try
        {
            applied = gameData.ApplyDocumentEdits("CommonFunctions", Name, edits, origin: this);
        }
        finally
        {
            committing = false;
            sourceRevision = resourceDocument?.Revision ?? 0;
        }
        if (applied)
            foreach (JsonDataEdit edit in edits)
                edit.Apply(data);
        graph.AcceptChanges();
        return applied;
    }

    public void Dispose()
    {
        if (resourceDocument is not null)
            resourceDocument.Changed -= onResourceChanged;
    }

    private void onResourceChanged(object? sender, EventArgs args)
    {
        if (committing || resourceDocument is null || sourceRevision == resourceDocument.Revision)
            return;
        sourceRevision = resourceDocument.Revision;
        EditorDocumentLiveChange? change = resourceDocument.LastChange;
        if (change is null || ReferenceEquals(change.Origin, this))
            return;
        if (change.Edits is IReadOnlyList<JsonDataEdit> edits && edits.All(edit => edit.CanApply(data)))
        {
            foreach (JsonDataEdit edit in edits)
                edit.Apply(data);
            if (edits.All(edit => edit.Kind == JsonDataEdit.Operation.Set && edit.Path.Count == 5
                && edit.Path[0] is "nodeGraph" && edit.Path[1] is "common" && edit.Path[2] is "nodes"
                && edit.Path[3] is int && edit.Path[4] is "params"))
                GraphParametersChanged?.Invoke(this, new BlueprintGraphParametersChangedEventArgs("common",
                    edits.Select(edit => (int)edit.Path[3]).Distinct().ToArray()));
            else
                ExternalChanged?.Invoke(this, EventArgs.Empty);
            return;
        }
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
