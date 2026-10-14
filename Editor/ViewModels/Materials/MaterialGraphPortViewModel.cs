using NodifyM.Avalonia.ViewModelBase;
using System;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels.Materials;

public sealed class MaterialGraphPortViewModel(MaterialGraphNodeViewModel node, JsonObject descriptor, bool input) : ConnectorViewModelBase
{
    public MaterialGraphNodeViewModel Node { get; } = node;
    public JsonObject Descriptor { get; } = descriptor;
    public string NodeId => Node.Id;
    public string Id => Descriptor["id"]!.GetValue<string>();
    public string ValueType => Descriptor["type"]!.GetValue<string>();
    public bool IsInput { get; } = input;
    public JsonNode? Value => Node.Data["inputs"]?[Id];
    public bool HasLink => Value is JsonObject link && link["node"] is JsonValue && link["port"] is JsonValue;
    public bool HasLiteral => !HasLink && Value is not null;
    public bool HasInlineValue => IsInput && !HasLink && (HasLiteral
        || Descriptor["default"] is not null && Descriptor["inherited"]?.GetValue<bool>() != true);
    public event EventHandler? InlineStateChanged;

    public void RefreshInline() => InlineStateChanged?.Invoke(this, EventArgs.Empty);

    public void Initialize()
    {
        Title = Id + " (" + ValueType + ")";
        Flow = IsInput ? ConnectorFlow.Input : ConnectorFlow.Output;
        CanConnect = true;
    }
}
