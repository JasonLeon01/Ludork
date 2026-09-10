using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class JsonSnapshotDictionary(IReadOnlyDictionary<string, JsonObject> source)
    : IReadOnlyDictionary<string, JsonObject>
{
    public JsonObject this[string key] => (JsonObject)source[key].DeepClone();
    public IEnumerable<string> Keys => source.Keys.ToArray();
    public IEnumerable<JsonObject> Values => this.Select(pair => pair.Value);
    public int Count => source.Count;

    public bool ContainsKey(string key) => source.ContainsKey(key);

    public bool TryGetValue(string key, out JsonObject value)
    {
        if (source.TryGetValue(key, out JsonObject? stored))
        {
            value = (JsonObject)stored.DeepClone();
            return true;
        }
        value = null!;
        return false;
    }

    public IEnumerator<KeyValuePair<string, JsonObject>> GetEnumerator()
    {
        foreach (string key in Keys)
            yield return new KeyValuePair<string, JsonObject>(key, this[key]);
    }

    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}
