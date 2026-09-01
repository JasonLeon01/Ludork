using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;

namespace Ludork.Models;

internal static class BlueprintGraphRawData
{
    public static JsonObject CloneWithout(
        JsonObject source,
        string first,
        string second,
        string? third = null,
        string? fourth = null,
        string? fifth = null)
    {
        JsonObject result = [];
        foreach (KeyValuePair<string, JsonNode?> pair in source)
        {
            if (string.Equals(pair.Key, first, StringComparison.Ordinal)
                || string.Equals(pair.Key, second, StringComparison.Ordinal)
                || third is not null && string.Equals(pair.Key, third, StringComparison.Ordinal)
                || fourth is not null && string.Equals(pair.Key, fourth, StringComparison.Ordinal)
                || fifth is not null && string.Equals(pair.Key, fifth, StringComparison.Ordinal))
            {
                continue;
            }
            result[pair.Key] = pair.Value?.DeepClone();
        }
        return result;
    }
}

public enum BlueprintGraphPortKind
{
    Exec,
    Params,
}

public enum BlueprintGraphPortDirection
{
    Input,
    Output,
}

public sealed class BlueprintGraphPortDefinition
{
    public BlueprintGraphPortDefinition(
        string name,
        BlueprintGraphPortKind kind,
        BlueprintGraphPortDirection direction,
        int pinIndex,
        string typeName = "any",
        int? parameterIndex = null,
        bool supportsEditor = false,
        JsonNode? defaultValue = null,
        JsonObject? meta = null)
    {
        Name = name;
        Kind = kind;
        Direction = direction;
        PinIndex = pinIndex;
        TypeName = typeName;
        ParameterIndex = parameterIndex;
        SupportsEditor = supportsEditor;
        DefaultValue = defaultValue?.DeepClone();
        Meta = meta?.DeepClone() as JsonObject ?? [];
    }

    public string Name { get; }
    public BlueprintGraphPortKind Kind { get; }
    public BlueprintGraphPortDirection Direction { get; }
    public int PinIndex { get; }
    public string TypeName { get; }
    public int? ParameterIndex { get; }
    public bool SupportsEditor { get; }
    public JsonNode? DefaultValue { get; }
    public JsonObject Meta { get; }
}

public sealed class BlueprintGraphNodeDefinition
{
    public BlueprintGraphNodeDefinition(
        string runtimePath,
        string title,
        IReadOnlyList<BlueprintGraphPortDefinition> ports,
        JsonObject? meta = null,
        IReadOnlyList<string>? runtimeAliases = null,
        IReadOnlyList<string>? pickerPath = null,
        string? memberName = null,
        string? description = null,
        LuaTypeReference? declaringType = null,
        bool isParent = false,
        bool isContextRelevant = false,
        bool hasExplicitDisplayName = false,
        bool isLatent = false)
    {
        RuntimePath = runtimePath;
        Title = title;
        Ports = ports;
        Meta = meta?.DeepClone() as JsonObject ?? [];
        RuntimeAliases = runtimeAliases ?? [runtimePath];
        PickerPath = pickerPath ?? [];
        MemberName = memberName ?? runtimePath.Split('.').LastOrDefault() ?? runtimePath;
        Description = description ?? string.Empty;
        DeclaringType = declaringType;
        IsParent = isParent;
        IsContextRelevant = isContextRelevant;
        HasExplicitDisplayName = hasExplicitDisplayName;
        IsLatent = isLatent;
    }

    public string RuntimePath { get; }
    public string Title { get; }
    public IReadOnlyList<BlueprintGraphPortDefinition> Ports { get; }
    public JsonObject Meta { get; }
    public IReadOnlyList<string> RuntimeAliases { get; }
    public IReadOnlyList<string> PickerPath { get; }
    public string MemberName { get; }
    public string Description { get; }
    public LuaTypeReference? DeclaringType { get; }
    public bool IsParent { get; }
    public bool IsContextRelevant { get; }
    public bool HasExplicitDisplayName { get; }
    public bool IsLatent { get; }
}

public sealed class BlueprintGraphEventParameterDefinition
{
    public BlueprintGraphEventParameterDefinition(
        string externalKey,
        string name,
        string typeName,
        int index)
    {
        ExternalKey = externalKey;
        Name = name;
        TypeName = typeName;
        Index = index;
    }

    public string ExternalKey { get; }
    public string Name { get; }
    public string TypeName { get; }
    public int Index { get; }
}

public sealed class BlueprintGraphEndpoint
{
    private BlueprintGraphEndpoint(Guid? nodeId, string? externalKey)
    {
        NodeId = nodeId;
        ExternalKey = externalKey;
    }

    public Guid? NodeId { get; }
    public string? ExternalKey { get; }
    public bool IsExternal => ExternalKey is not null;

    public static BlueprintGraphEndpoint Node(Guid nodeId)
    {
        return new BlueprintGraphEndpoint(nodeId, null);
    }

    public static BlueprintGraphEndpoint External(string externalKey, Guid nodeId)
    {
        return new BlueprintGraphEndpoint(nodeId, externalKey);
    }
}

public sealed class BlueprintGraphPort : INotifyPropertyChanged
{
    private JsonNode? value;
    private int connectionCount;
    private bool isValueModified;

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
    public bool IsEditorVisible => Direction == BlueprintGraphPortDirection.Input
        && Kind == BlueprintGraphPortKind.Params
        && SupportsEditor
        && !IsConnected;
    public JsonNode? Value
    {
        get => value;
        set
        {
            if (JsonNode.DeepEquals(this.value, value))
                return;
            this.value = value?.DeepClone();
            isValueModified = true;
            notifyPropertyChanged();
            notifyPropertyChanged(nameof(IsValueModified));
        }
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

public sealed class BlueprintGraphConnection
{
    public BlueprintGraphConnection(
        Guid id,
        int? originalIndex,
        BlueprintGraphEndpoint source,
        BlueprintGraphEndpoint target,
        Guid sourcePortId,
        Guid targetPortId,
        BlueprintGraphPortKind kind,
        int sourcePinIndex,
        int targetPinIndex,
        JsonObject rawData)
    {
        Id = id;
        OriginalIndex = originalIndex;
        Source = source;
        Target = target;
        SourcePortId = sourcePortId;
        TargetPortId = targetPortId;
        Kind = kind;
        SourcePinIndex = sourcePinIndex;
        TargetPinIndex = targetPinIndex;
        RawData = BlueprintGraphRawData.CloneWithout(
            rawData,
            "left",
            "right",
            "leftOutPin",
            "rightInPin",
            "linkType");
    }

    public Guid Id { get; }
    public int? OriginalIndex { get; }
    public BlueprintGraphEndpoint Source { get; }
    public BlueprintGraphEndpoint Target { get; }
    public Guid SourcePortId { get; }
    public Guid TargetPortId { get; }
    public BlueprintGraphPortKind Kind { get; }
    public int SourcePinIndex { get; }
    public int TargetPinIndex { get; }
    public JsonObject RawData { get; }
}

public sealed class BlueprintGraphDocument
{
    private BlueprintGraphEndpoint? start;

    public BlueprintGraphDocument(string eventName, JsonObject rawEventGraph)
    {
        EventName = eventName;
        RawEventGraph = BlueprintGraphRawData.CloneWithout(rawEventGraph, "nodes", "links");
    }

    public event EventHandler? Changed;

    public string EventName { get; }
    public JsonObject RawEventGraph { get; }
    public ObservableCollection<BlueprintGraphNode> Nodes { get; } = [];
    public ObservableCollection<BlueprintGraphConnection> Connections { get; } = [];
    public BlueprintGraphEndpoint? Start
    {
        get => start;
        set
        {
            if (start?.NodeId == value?.NodeId
                && string.Equals(start?.ExternalKey, value?.ExternalKey, StringComparison.Ordinal))
            {
                return;
            }
            if (start?.NodeId is Guid previousId && FindNode(previousId) is BlueprintGraphNode previous)
                previous.IsStart = false;
            start = value;
            if (start?.NodeId is Guid nextId && FindNode(nextId) is BlueprintGraphNode next)
                next.IsStart = true;
            Changed?.Invoke(this, EventArgs.Empty);
        }
    }

    public BlueprintGraphNode? FindNode(Guid id)
    {
        return Nodes.FirstOrDefault(node => node.Id == id);
    }

    public BlueprintGraphPort? FindPort(Guid id)
    {
        foreach (BlueprintGraphNode node in Nodes)
        {
            BlueprintGraphPort? port = node.Inputs.Concat(node.Outputs)
                .FirstOrDefault(value => value.Id == id);
            if (port is not null)
                return port;
        }
        return null;
    }

    public bool AddConnection(BlueprintGraphConnection connection)
    {
        BlueprintGraphPort? source = FindPort(connection.SourcePortId);
        BlueprintGraphPort? target = FindPort(connection.TargetPortId);
        if (source is null || target is null
            || source.Direction != BlueprintGraphPortDirection.Output
            || target.Direction != BlueprintGraphPortDirection.Input
            || source.Kind != target.Kind
            || source.Kind != connection.Kind
            || target.ConnectionCount > 0)
        {
            return false;
        }
        Connections.Add(connection);
        source.AttachConnection();
        target.AttachConnection();
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool RemoveConnection(Guid id)
    {
        BlueprintGraphConnection? connection = Connections.FirstOrDefault(value => value.Id == id);
        if (connection is null)
            return false;
        FindPort(connection.SourcePortId)?.DetachConnection();
        FindPort(connection.TargetPortId)?.DetachConnection();
        Connections.Remove(connection);
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    internal void AddLoadedConnection(BlueprintGraphConnection connection)
    {
        Connections.Add(connection);
        FindPort(connection.SourcePortId)?.AttachConnection();
        FindPort(connection.TargetPortId)?.AttachConnection();
    }

    public void NotifyChanged()
    {
        Changed?.Invoke(this, EventArgs.Empty);
    }
}

public sealed class BlueprintGraphSaveResult
{
    public BlueprintGraphSaveResult(JsonObject eventGraph, JsonNode? startNode)
    {
        EventGraph = eventGraph;
        StartNode = startNode?.DeepClone();
    }

    public JsonObject EventGraph { get; }
    public JsonNode? StartNode { get; }
}
