using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphDocument
{
    private BlueprintGraphEndpoint? start;

    public BlueprintGraphDocument(string eventName, JsonObject rawEventGraph)
    {
        EventName = eventName;
        RawEventGraph = (JsonObject)rawEventGraph.DeepClone();
    }

    public event EventHandler? Changed;

    public string EventName { get; }
    public Func<string, string, bool>? IsTypeAssignable { get; set; }

    public bool ArePortTypesCompatible(BlueprintGraphPort source, BlueprintGraphPort target)
    {
        return source.Kind == target.Kind && (source.Kind == BlueprintGraphPortKind.Exec
            || (IsTypeAssignable?.Invoke(source.TypeName, target.TypeName)
                ?? LuaMetadataType.Parse(source.TypeName).IsAssignableTo(LuaMetadataType.Parse(target.TypeName))));
    }
    public static BlueprintGraphPort GetExclusivePin(BlueprintGraphPort source, BlueprintGraphPort target)
    {
        return source.Kind == BlueprintGraphPortKind.Exec ? source : target;
    }
    public JsonObject RawEventGraph { get; }
    public ObservableCollection<BlueprintGraphNode> Nodes { get; } = [];
    public ObservableCollection<BlueprintGraphConnection> Connections { get; } = [];
    public ObservableCollection<BlueprintGraphUnresolvedConnection> UnresolvedConnections { get; } = [];
    internal JsonNode? UnresolvedStartNode { get; private set; }
    public BlueprintGraphEndpoint? Start
    {
        get => start;
        set
        {
            if (start?.NodeId == value?.NodeId
                && string.Equals(start?.ExternalKey, value?.ExternalKey, StringComparison.Ordinal)
                && UnresolvedStartNode is null)
            {
                return;
            }
            if (start?.NodeId is Guid previousId && FindNode(previousId) is BlueprintGraphNode previous)
                previous.IsStart = false;
            start = value;
            UnresolvedStartNode = null;
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
            || !ArePortTypesCompatible(source, target)
            || GetExclusivePin(source, target).ConnectionCount > 0)
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

    internal void SetLoadedStart(BlueprintGraphEndpoint? value, JsonNode? rawStart)
    {
        Start = value;
        UnresolvedStartNode = value is null ? rawStart?.DeepClone() : null;
    }

    public void NotifyChanged()
    {
        Changed?.Invoke(this, EventArgs.Empty);
    }
}
