using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using Ludork.Models;

namespace Ludork.Services;

internal sealed class DocumentSnapshotDictionary<T>(
    IReadOnlyDictionary<string, JsonObject> source,
    Func<JsonObject, T> create) : IReadOnlyDictionary<string, T> where T : IEditorDataSnapshot
{
    public T this[string key] => create(source[key]);
    public IEnumerable<string> Keys => source.Keys.ToArray();
    public IEnumerable<T> Values => Keys.Select(key => this[key]);
    public int Count => source.Count;
    public bool ContainsKey(string key) => source.ContainsKey(key);
    public bool TryGetValue(string key, out T value)
    {
        if (source.TryGetValue(key, out JsonObject? data))
        {
            value = create(data);
            return true;
        }
        value = default!;
        return false;
    }
    public IEnumerator<KeyValuePair<string, T>> GetEnumerator()
        => source.Select(pair => new KeyValuePair<string, T>(pair.Key, create(pair.Value))).GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();
}
