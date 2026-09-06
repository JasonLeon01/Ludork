using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphNode : INotifyPropertyChanged
{
    private double x;
    private double y;
    private bool isStart;

    public BlueprintGraphNode(
        Guid id,
        int? originalIndex,
        string nodeFunction,
        string title,
        double x,
        double y,
        bool isResolved,
        bool isVirtual,
        string? externalKey,
        JsonObject rawData,
        JsonArray parameters,
        string? description = null)
    {
        Id = id;
        OriginalIndex = originalIndex;
        NodeFunction = nodeFunction;
        Title = title;
        this.x = x;
        this.y = y;
        IsResolved = isResolved;
        IsVirtual = isVirtual;
        ExternalKey = externalKey;
        RawData = BlueprintGraphRawData.CloneWithout(rawData, "nodeFunction", "params", "pos");
        Parameters = (JsonArray)parameters.DeepClone();
        Description = description ?? string.Empty;
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public Guid Id { get; }
    public int? OriginalIndex { get; }
    public string NodeFunction { get; set; }
    public string Title { get; set; }
    public bool IsResolved { get; }
    public bool IsVirtual { get; }
    public string? ExternalKey { get; }
    public JsonObject RawData { get; }
    public JsonArray Parameters { get; }
    public string Description { get; }
    public ObservableCollection<BlueprintGraphPort> Inputs { get; } = [];
    public ObservableCollection<BlueprintGraphPort> Outputs { get; } = [];
    public double X
    {
        get => x;
        set
        {
            if (Math.Abs(x - value) < double.Epsilon)
                return;
            x = value;
            notifyPropertyChanged();
        }
    }
    public double Y
    {
        get => y;
        set
        {
            if (Math.Abs(y - value) < double.Epsilon)
                return;
            y = value;
            notifyPropertyChanged();
        }
    }
    public bool IsStart
    {
        get => isStart;
        internal set
        {
            if (isStart == value)
                return;
            isStart = value;
            notifyPropertyChanged();
        }
    }

    public BlueprintGraphPort? FindPort(
        BlueprintGraphPortDirection direction,
        BlueprintGraphPortKind kind,
        int pinIndex)
    {
        IEnumerable<BlueprintGraphPort> ports = direction == BlueprintGraphPortDirection.Input
            ? Inputs
            : Outputs;
        return ports.FirstOrDefault(port => port.Kind == kind && port.PinIndex == pinIndex);
    }

    internal void AddPort(BlueprintGraphPort port)
    {
        if (port.Direction == BlueprintGraphPortDirection.Input)
            Inputs.Add(port);
        else
            Outputs.Add(port);
    }

    private void notifyPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}
