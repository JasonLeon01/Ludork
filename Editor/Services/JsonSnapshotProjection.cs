using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

internal sealed class JsonSnapshotProjection<T>(IReadOnlyDictionary<string, T> snapshots)
    : IReadOnlyDictionary<string, JsonObject> where T : IEditorDataSnapshot
{
    public JsonObject this[string key] => snapshots[key].ToJson();
    public IEnumerable<string> Keys => snapshots.Keys;
    public IEnumerable<JsonObject> Values => snapshots.Values.Select(value => value.ToJson());
    public int Count => snapshots.Count;
    public bool ContainsKey(string key) => snapshots.ContainsKey(key);
    public bool TryGetValue(string key, out JsonObject value)
    {
        if (snapshots.TryGetValue(key, out T? snapshot))
        {
            value = snapshot.ToJson();
            return true;
        }
        value = null!;
        return false;
    }
    public IEnumerator<KeyValuePair<string, JsonObject>> GetEnumerator()
        => snapshots.Select(pair => new KeyValuePair<string, JsonObject>(pair.Key, pair.Value.ToJson())).GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}
