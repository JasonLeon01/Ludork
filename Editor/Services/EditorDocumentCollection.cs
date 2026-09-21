using Ludork.Models;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Services;

internal sealed class EditorDocumentCollection : IDictionary<string, JsonObject>, IReadOnlyDictionary<string, JsonObject>
{
    private readonly HashSet<string>? acceptedTypes;

    public EditorDocumentCollection(string? expectedType, bool writeType)
        : this(expectedType, writeType, true)
    {
    }

    public EditorDocumentCollection(string? expectedType, bool writeType, bool persist)
    {
        ExpectedType = expectedType;
        WriteType = writeType;
        Persist = persist;
    }

    public EditorDocumentCollection(IEnumerable<string> acceptedTypes)
    {
        this.acceptedTypes = new HashSet<string>(acceptedTypes, StringComparer.Ordinal);
        PreserveType = true;
    }

    public string? ExpectedType { get; }
    public bool WriteType { get; }
    public bool PreserveType { get; }
    public bool Persist { get; } = true;
    private readonly Dictionary<string, JsonObject> data = new(StringComparer.Ordinal);
    private Action<string, string?, HistoryMarker?>? record;

    public JsonObject this[string key] { get => data[key]; set => data[key] = value; }
    public ICollection<string> Keys => data.Keys;
    public ICollection<JsonObject> Values => data.Values;
    IEnumerable<string> IReadOnlyDictionary<string, JsonObject>.Keys => data.Keys;
    IEnumerable<JsonObject> IReadOnlyDictionary<string, JsonObject>.Values => data.Values;
    public int Count => data.Count;
    public bool IsReadOnly => false;
    public void Add(string key, JsonObject value) => data.Add(key, value);
    public bool ContainsKey(string key) => data.ContainsKey(key);
    public bool Remove(string key) => data.Remove(key);
    public bool TryGetValue(string key, out JsonObject value) => data.TryGetValue(key, out value!);
    public void Add(KeyValuePair<string, JsonObject> item) => ((ICollection<KeyValuePair<string, JsonObject>>)data).Add(item);
    public bool Contains(KeyValuePair<string, JsonObject> item) => ((ICollection<KeyValuePair<string, JsonObject>>)data).Contains(item);
    public void CopyTo(KeyValuePair<string, JsonObject>[] array, int index) => ((ICollection<KeyValuePair<string, JsonObject>>)data).CopyTo(array, index);
    public bool Remove(KeyValuePair<string, JsonObject> item) => ((ICollection<KeyValuePair<string, JsonObject>>)data).Remove(item);
    public void Clear() => data.Clear();
    public IEnumerator<KeyValuePair<string, JsonObject>> GetEnumerator() => data.GetEnumerator();
    IEnumerator IEnumerable.GetEnumerator() => GetEnumerator();

    internal void BindHistory(Action<string, string?, HistoryMarker?> recorder) => record = recorder;
    internal void RecordChange(string key, string? description = null, HistoryMarker? marker = null)
    {
        if (record is null)
            throw new InvalidOperationException("The document collection is not attached to a project.");
        record(key, description, marker);
    }
    private JsonSnapshotDictionary? snapshots;
    public JsonSnapshotDictionary Snapshots => snapshots ??= new JsonSnapshotDictionary(this);

    public bool AcceptsType(string? type)
    {
        if (acceptedTypes is not null)
            return type is not null && acceptedTypes.Contains(type);
        return ExpectedType is null
            || type is null
            || string.Equals(type, ExpectedType, StringComparison.Ordinal);
    }
}
