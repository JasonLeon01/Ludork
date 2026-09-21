using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public abstract class EditorJsonSnapshot(JsonObject source) : EditorDataSnapshot(source)
{
    protected JsonObject ReadObject(string name) => SnapshotData[name]?.DeepClone() as JsonObject ?? [];
    protected JsonArray ReadArray(string name) => SnapshotData[name]?.DeepClone() as JsonArray ?? [];
    protected bool HasProperty(string name) => SnapshotData.ContainsKey(name);
    protected string? ReadString(string name) => SnapshotData[name] is JsonValue value && value.TryGetValue(out string? text) ? text : null;
    protected bool ReadBoolean(string name) => SnapshotData[name] is JsonValue value && value.TryGetValue(out bool result) && result;
    protected double? ReadNumber(string name) => ReadFiniteNumber(SnapshotData[name]);

    protected static double? ReadFiniteNumber(JsonNode? value)
    {
        if (value is not JsonValue scalar || scalar.GetValueKind() != JsonValueKind.Number)
            return null;
        double number = MapSnapshotValues.Number(scalar, double.NaN);
        return double.IsFinite(number) ? number : null;
    }

    protected int? ReadInteger(string name)
    {
        double? value = ReadNumber(name);
        return value is double number && number == Math.Truncate(number) && number >= int.MinValue && number <= int.MaxValue
            ? (int)number
            : null;
    }

    protected IReadOnlyDictionary<string, T> ReadObjects<T>(string name, Func<JsonObject, T> factory)
    {
        return SnapshotData[name] is JsonObject objects
            ? objects.Where(entry => entry.Value is JsonObject).ToDictionary(entry => entry.Key, entry => factory((JsonObject)entry.Value!), StringComparer.Ordinal)
            : new Dictionary<string, T>(StringComparer.Ordinal);
    }
}
