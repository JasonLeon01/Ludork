using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class BlueprintVariableDependency
{
    public BlueprintVariableDependency(string source, JsonNode? expectedValue, string operation = "==")
    {
        Source = source;
        ExpectedValue = expectedValue?.DeepClone();
        Operator = operation == "!=" ? "!=" : "==";
    }

    public string Source { get; }
    public JsonNode? ExpectedValue { get; }
    public string Operator { get; }

    public BlueprintVariableDependency Clone()
    {
        return new BlueprintVariableDependency(Source, ExpectedValue, Operator);
    }
}
