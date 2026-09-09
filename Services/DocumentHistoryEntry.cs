using System.Text.Json.Nodes;

namespace Ludork.Services;

public sealed class EditorDocumentState
{
    internal EditorDocumentState(string section, string key, string path, JsonObject? data)
    {
        Section = section;
        Key = key;
        Path = path;
        InternalData = data?.DeepClone() as JsonObject;
    }

    public string Section { get; }
    public string Key { get; }
    public string Path { get; }
    public JsonObject? Data => InternalData?.DeepClone() as JsonObject;
    internal JsonObject? InternalData { get; }
}

public sealed record DocumentHistoryEntry(
    EditorDocumentState Before,
    EditorDocumentState After,
    string? Description = null,
    HistoryMarker? Marker = null)
{
    internal long GestureId { get; init; }
}
