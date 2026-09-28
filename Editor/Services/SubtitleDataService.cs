using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class SubtitleDataService
{
    private readonly ProjectDataStore store;
    private readonly EditorDocumentCollection documents;

    internal SubtitleDataService(ProjectDataStore store, EditorDocumentCollection documents)
    {
        this.store = store;
        this.documents = documents;
    }

    public IEnumerable<string> Keys => documents.Keys;
    public bool Contains(string key) => documents.ContainsKey(ProjectDataStore.normalizeJsonKey(key));

    public JsonObject? GetSubtitle(string key) => documents.TryGetValue(ProjectDataStore.normalizeJsonKey(key), out JsonObject? data)
        ? (JsonObject)data.DeepClone() : null;

    public bool CreateSubtitle(string key)
    {
        key = ProjectDataStore.normalizeJsonKey(key);
        if (key.Split('/').Any(part => part.Length == 0 || part is "." or "..")
            || !store.canCreateDocument("Subtitles", key))
            return false;
        documents.RecordChange(key);
        documents[key] = SubtitleAssetSchema.Create();
        store.refreshModifiedState();
        return true;
    }

    public bool UpdateSubtitle(string key, JsonObject data)
    {
        key = ProjectDataStore.normalizeJsonKey(key);
        if (!documents.TryGetValue(key, out JsonObject? current) || JsonNode.DeepEquals(current, data))
            return false;
        documents.RecordChange(key);
        documents[key] = (JsonObject)data.DeepClone();
        store.refreshModifiedState();
        return true;
    }
}
