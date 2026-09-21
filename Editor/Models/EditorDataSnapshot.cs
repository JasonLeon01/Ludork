using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public abstract class EditorDataSnapshot : IEditorDataSnapshot
{
    protected EditorDataSnapshot(JsonObject data) => SnapshotData = (JsonObject)data.DeepClone();
    protected JsonObject SnapshotData { get; }
    public JsonObject ToJson() => (JsonObject)SnapshotData.DeepClone();
    protected string Text(string name, string fallback = "") => SnapshotData[name] is JsonValue value && value.TryGetValue(out string? text) ? text : fallback;
    protected double Number(string name, double fallback = 0) => double.TryParse(SnapshotData[name]?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double value) ? value : fallback;
    protected bool Boolean(string name, bool fallback = false) => SnapshotData[name] is JsonValue value && value.TryGetValue(out bool result) ? result : fallback;
    protected JsonNode? ReadValue(string name) => SnapshotData[name]?.DeepClone();
    protected IReadOnlyList<T> Objects<T>(string name, Func<JsonObject, T> create) => (SnapshotData[name] as JsonArray ?? []).OfType<JsonObject>().Select(create).ToArray();
    protected IReadOnlyList<string> Strings(string name) => (SnapshotData[name] as JsonArray ?? []).Select(value => value?.GetValue<string>() ?? string.Empty).ToArray();
    protected IReadOnlyList<double> Numbers(string name) => (SnapshotData[name] as JsonArray ?? []).Select(value => double.TryParse(value?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double number) ? number : 0).ToArray();
}
