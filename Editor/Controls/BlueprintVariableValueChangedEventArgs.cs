using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class BlueprintVariableValueChangedEventArgs(
    string name,
    JsonNode? value,
    bool requiresRefresh,
    IReadOnlyList<string>? path = null) : EventArgs
{
    public string Name { get; } = name;
    public JsonNode? Value { get; } = value;
    public bool RequiresRefresh { get; } = requiresRefresh;
    public IReadOnlyList<string> Path { get; } = path ?? [];
}
