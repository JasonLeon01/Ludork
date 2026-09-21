using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintVariableOption
{
    public BlueprintVariableOption(string label, JsonNode? value)
    {
        Label = label;
        Value = value?.DeepClone();
    }

    public string Label { get; }
    public JsonNode? Value { get; }

    public BlueprintVariableOption Clone()
    {
        return new BlueprintVariableOption(Label, Value);
    }

    public override string ToString()
    {
        return Label;
    }
}
