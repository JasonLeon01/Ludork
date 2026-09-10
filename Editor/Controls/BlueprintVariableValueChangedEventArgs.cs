using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class BlueprintVariableValueChangedEventArgs(
    string name,
    JsonNode? value,
    bool requiresRefresh) : EventArgs
{
    public string Name { get; } = name;
    public JsonNode? Value { get; } = value;
    public bool RequiresRefresh { get; } = requiresRefresh;
}
