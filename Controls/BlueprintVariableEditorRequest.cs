using System;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class BlueprintVariableEditorRequest
{
    public BlueprintVariableEditorRequest(
        BlueprintVariableField field,
        JsonNode? value,
        Action<JsonNode?, bool> commit)
    {
        Field = field;
        Value = value?.DeepClone();
        Commit = commit;
    }

    public BlueprintVariableField Field { get; }
    public JsonNode? Value { get; }
    public Action<JsonNode?, bool> Commit { get; }
}
