using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class BlueprintProgressValueChangedEventArgs(JsonNode? value) : EventArgs
{
    public JsonNode? Value { get; } = value;
}
