using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class GameVariableDefinition
{
    private readonly JsonNode? initialValue;

    public GameVariableDefinition(
        string name,
        GameVariableType type,
        JsonNode? initialValue = null,
        string? remark = null)
    {
        Name = name;
        Type = type;
        this.initialValue = initialValue?.DeepClone();
        Remark = remark ?? string.Empty;
    }

    public string Name { get; }
    public GameVariableType Type { get; }
    public JsonNode? InitialValue => initialValue?.DeepClone();
    public string Remark { get; }
}
