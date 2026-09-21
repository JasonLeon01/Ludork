using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class GeneralDataTypeSnapshot(JsonObject source) : EditorJsonSnapshot(source)
{
    public IReadOnlyDictionary<string, GeneralDataParameterSnapshot> Parameters => ReadObjects("params", value => new GeneralDataParameterSnapshot(value));
    public IReadOnlyDictionary<string, JsonObject> Members => ReadObjects("members", value => (JsonObject)value.DeepClone());
    public IReadOnlyList<string> Events => ReadArray("events").OfType<JsonValue>()
        .Select(value => value.TryGetValue(out string? text) ? text : null).Where(text => !string.IsNullOrWhiteSpace(text)).Cast<string>().ToArray();
    public bool HasParameters => HasProperty("params");
    public JsonObject ParameterDefinitions => ReadObject("params");
    internal void ApplyMemberValue(string memberId, string name, JsonNode? value)
    {
        if (SnapshotData["members"]?[memberId] is JsonObject member)
            member[name] = value?.DeepClone();
    }
}
