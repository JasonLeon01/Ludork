using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphPort : INotifyPropertyChanged
{
    private JsonNode? value;
    private int connectionCount;
    private bool isValueModified;
    private string? valueDiagnostic;

    public BlueprintGraphPort(
        Guid id,
        Guid nodeId,
        string name,
        BlueprintGraphPortKind kind,
        BlueprintGraphPortDirection direction,
        int pinIndex,
        string typeName,
        int? parameterIndex,
        bool supportsEditor,
        JsonNode? value,
        JsonObject? meta = null)
    {
        Id = id;
        NodeId = nodeId;
        Name = name;
        Kind = kind;
        Direction = direction;
        PinIndex = pinIndex;
        TypeName = typeName;
        ParameterIndex = parameterIndex;
        SupportsEditor = supportsEditor;
        this.value = value?.DeepClone();
        Meta = meta?.DeepClone() as JsonObject ?? [];
        updateValueDiagnostic();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public Guid Id { get; }
    public Guid NodeId { get; }
    public string Name { get; }
    public BlueprintGraphPortKind Kind { get; }
    public BlueprintGraphPortDirection Direction { get; }
    public int PinIndex { get; }
    public string TypeName { get; }
    public int? ParameterIndex { get; }
    public bool SupportsEditor { get; }
    public JsonObject Meta { get; }
    public bool IsValueModified => isValueModified;
    public int ConnectionCount => connectionCount;
    public bool IsConnected => connectionCount > 0;
    public string? ValueDiagnostic => valueDiagnostic;
    public bool IsEditorVisible => Direction == BlueprintGraphPortDirection.Input
        && Kind == BlueprintGraphPortKind.Params
        && SupportsEditor
        && (!IsConnected || ValueDiagnostic is not null);
    public JsonNode? Value
    {
        get => value;
        set
        {
            if (JsonNode.DeepEquals(this.value, value))
                return;
            this.value = value?.DeepClone();
            isValueModified = true;
            updateValueDiagnostic();
            notifyPropertyChanged();
            notifyPropertyChanged(nameof(IsValueModified));
            notifyPropertyChanged(nameof(ValueDiagnostic));
            notifyPropertyChanged(nameof(IsEditorVisible));
        }
    }

    private void updateValueDiagnostic()
    {
        valueDiagnostic = null;
        if (Direction != BlueprintGraphPortDirection.Input || Kind != BlueprintGraphPortKind.Params)
            return;
        List<string> errors = [];
        LuaMetadataLiteralValidation.ValidateNodeParameter(LuaMetadataType.Parse(TypeName), value, Name, errors);
        if (errors.Count != 0)
            valueDiagnostic = string.Join(Environment.NewLine, errors);
    }

    internal void AttachConnection()
    {
        connectionCount++;
        notifyConnectionChanged();
    }

    internal void DetachConnection()
    {
        if (connectionCount == 0)
            return;
        connectionCount--;
        notifyConnectionChanged();
    }

    private void notifyConnectionChanged()
    {
        notifyPropertyChanged(nameof(ConnectionCount));
        notifyPropertyChanged(nameof(IsConnected));
        notifyPropertyChanged(nameof(IsEditorVisible));
    }

    private void notifyPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}
