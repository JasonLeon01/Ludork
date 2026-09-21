using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class GeneralDataParameterSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public JsonNode? TypeSchema => ReadValue("type");
    public JsonNode? ItemTypeSchema => ReadValue("itemType");
    public JsonNode? ValueTypeSchema => ReadValue("valueType");
    public bool HasDefault => HasProperty("defaultValue");
    public JsonNode? DefaultValue => ReadValue("defaultValue");
    public string Comment => ReadString("comment") ?? string.Empty;
    public string BasePath => ReadString("base") ?? string.Empty;
    public JsonObject Reference => ReadObject("reference");
}
