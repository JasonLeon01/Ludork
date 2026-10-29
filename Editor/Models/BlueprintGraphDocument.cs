using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Collections.Specialized;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphDocument
{
    private BlueprintGraphEndpoint? start;
    private int updateDepth;
    private bool pendingChange;
    private bool structuralChange;
    private readonly HashSet<Guid> changedParameterNodes = [];
    private readonly Dictionary<Guid, int> savedNodeIndices = [];
    private readonly Dictionary<int, BlueprintGraphNode> savedNodes = [];
    private readonly Dictionary<Guid, BlueprintGraphNode> nodesById = [];
    private readonly Dictionary<Guid, BlueprintGraphPort> portsById = [];
    private readonly Dictionary<Guid, BlueprintGraphConnection> connectionsById = [];
    private readonly Dictionary<Guid, HashSet<Guid>> connectionsByPort = [];
    private readonly HashSet<BlueprintGraphNode> indexedNodes = [];
    private int entryConnectionCount;

    public BlueprintGraphDocument(string eventName, JsonObject rawEventGraph)
    {
        EventName = eventName;
        RawEventGraph = (JsonObject)rawEventGraph.DeepClone();
        Nodes.CollectionChanged += onNodesChanged;
        Connections.CollectionChanged += onConnectionsChanged;
    }

    public event EventHandler? Changed;
    internal event EventHandler? EntryConnectionChanged;
    internal bool HasEntryConnection => entryConnectionCount != 0;

    public string EventName { get; }
    public bool InheritsEvents { get; init; }
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
            start = value;
            UnresolvedStartNode = null;
            NotifyChanged();
        }
    }

    public BlueprintGraphNode? FindNode(Guid id) => nodesById.GetValueOrDefault(id);

    public BlueprintGraphPort? FindPort(Guid id) => portsById.GetValueOrDefault(id);

    internal IEnumerable<BlueprintGraphConnection> GetPortConnections(Guid portId)
    {
        return connectionsByPort.TryGetValue(portId, out HashSet<Guid>? ids)
            ? ids.Select(id => connectionsById[id]) : [];
    }

    private void onNodesChanged(object? sender, NotifyCollectionChangedEventArgs args)
    {
        if (args.Action == NotifyCollectionChangedAction.Move)
            return;
        if (args.Action == NotifyCollectionChangedAction.Reset)
        {
            foreach (BlueprintGraphNode node in indexedNodes.ToArray())
                removeNodeIndex(node);
            foreach (BlueprintGraphNode node in Nodes)
                addNodeIndex(node);
            return;
        }
        if (args.OldItems is not null)
            foreach (BlueprintGraphNode node in args.OldItems)
                removeNodeIndex(node);
        if (args.NewItems is not null)
            foreach (BlueprintGraphNode node in args.NewItems)
                addNodeIndex(node);
    }

    private void addNodeIndex(BlueprintGraphNode node)
    {
        nodesById[node.Id] = node;
        indexedNodes.Add(node);
        foreach (BlueprintGraphPort port in node.Inputs.Concat(node.Outputs))
            portsById[port.Id] = port;
        node.Inputs.CollectionChanged += onPortsChanged;
        node.Outputs.CollectionChanged += onPortsChanged;
    }

    private void removeNodeIndex(BlueprintGraphNode node)
    {
        node.Inputs.CollectionChanged -= onPortsChanged;
        node.Outputs.CollectionChanged -= onPortsChanged;
        indexedNodes.Remove(node);
        nodesById.Remove(node.Id);
        foreach (BlueprintGraphPort port in node.Inputs.Concat(node.Outputs))
            portsById.Remove(port.Id);
    }

    private void onPortsChanged(object? sender, NotifyCollectionChangedEventArgs args)
    {
        if (args.Action == NotifyCollectionChangedAction.Move)
            return;
        if (args.Action == NotifyCollectionChangedAction.Reset)
        {
            portsById.Clear();
            foreach (BlueprintGraphNode node in Nodes)
                foreach (BlueprintGraphPort port in node.Inputs.Concat(node.Outputs))
                    portsById[port.Id] = port;
            return;
        }
        if (args.OldItems is not null)
            foreach (BlueprintGraphPort port in args.OldItems)
                portsById.Remove(port.Id);
        if (args.NewItems is not null)
            foreach (BlueprintGraphPort port in args.NewItems)
                portsById[port.Id] = port;
    }

    private void onConnectionsChanged(object? sender, NotifyCollectionChangedEventArgs args)
    {
        if (args.Action == NotifyCollectionChangedAction.Move)
            return;
        bool hadEntry = HasEntryConnection;
        if (args.Action == NotifyCollectionChangedAction.Reset)
        {
            connectionsById.Clear();
            connectionsByPort.Clear();
            entryConnectionCount = 0;
            foreach (BlueprintGraphConnection connection in Connections)
                addConnectionIndex(connection);
        }
        else
        {
            if (args.OldItems is not null)
                foreach (BlueprintGraphConnection connection in args.OldItems)
                {
                    connectionsById.Remove(connection.Id);
                    removePortConnection(connection.SourcePortId, connection.Id);
                    removePortConnection(connection.TargetPortId, connection.Id);
                    if (connection.IsEntryConnection)
                        entryConnectionCount--;
                }
            if (args.NewItems is not null)
                foreach (BlueprintGraphConnection connection in args.NewItems)
                    addConnectionIndex(connection);
        }
        if (hadEntry != HasEntryConnection)
            EntryConnectionChanged?.Invoke(this, EventArgs.Empty);
    }

    private void addConnectionIndex(BlueprintGraphConnection connection)
    {
        connectionsById[connection.Id] = connection;
        addPortConnection(connection.SourcePortId, connection.Id);
        addPortConnection(connection.TargetPortId, connection.Id);
        if (connection.IsEntryConnection)
            entryConnectionCount++;
    }

    private void addPortConnection(Guid portId, Guid connectionId)
    {
        if (!connectionsByPort.TryGetValue(portId, out HashSet<Guid>? ids))
            connectionsByPort[portId] = ids = [];
        ids.Add(connectionId);
    }

    private void removePortConnection(Guid portId, Guid connectionId)
    {
        if (connectionsByPort.TryGetValue(portId, out HashSet<Guid>? ids)
            && ids.Remove(connectionId) && ids.Count == 0)
            connectionsByPort.Remove(portId);
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
        if (connection.IsEntryConnection)
        {
            start = connection.Target;
            UnresolvedStartNode = null;
        }
        NotifyChanged();
        return true;
    }

    public bool RemoveConnection(Guid id)
    {
        BlueprintGraphConnection? connection = connectionsById.GetValueOrDefault(id);
        if (connection is null)
            return false;
        FindPort(connection.SourcePortId)?.DetachConnection();
        FindPort(connection.TargetPortId)?.DetachConnection();
        Connections.Remove(connection);
        if (connection.IsEntryConnection)
        {
            start = null;
            UnresolvedStartNode = null;
        }
        NotifyChanged();
        return true;
    }

    internal void AddLoadedConnection(BlueprintGraphConnection connection,
        BlueprintGraphPort source, BlueprintGraphPort target)
    {
        Connections.Add(connection);
        source.AttachConnection();
        target.AttachConnection();
    }

    internal void SetLoadedStart(BlueprintGraphEndpoint? value, JsonNode? rawStart)
    {
        start = value;
        UnresolvedStartNode = value is null ? rawStart?.DeepClone() : null;
    }

    public void NotifyChanged()
    {
        structuralChange = true;
        publishChanged();
    }

    internal void NotifyParameterChanged(BlueprintGraphPort port)
    {
        changedParameterNodes.Add(port.NodeId);
        publishChanged();
    }

    internal IReadOnlyList<(BlueprintGraphNode Node, int Index)>? GetParameterChanges()
    {
        if (structuralChange)
            return null;
        List<(BlueprintGraphNode Node, int Index)> changes = [];
        foreach (Guid id in changedParameterNodes)
        {
            if (!nodesById.TryGetValue(id, out BlueprintGraphNode? node)
                || !savedNodeIndices.TryGetValue(id, out int index))
                return null;
            changes.Add((node, index));
        }
        return changes;
    }

    internal void AcceptChanges()
    {
        if (structuralChange || savedNodeIndices.Count == 0)
        {
            savedNodeIndices.Clear();
            savedNodes.Clear();
            foreach (BlueprintGraphNode node in Nodes.Where(node => !node.IsVirtual))
            {
                savedNodes[savedNodeIndices.Count] = node;
                savedNodeIndices[node.Id] = savedNodeIndices.Count;
            }
        }
        structuralChange = false;
        changedParameterNodes.Clear();
    }

    internal BlueprintGraphNode? FindSavedNode(int index)
        => savedNodes.GetValueOrDefault(index);

    private void publishChanged()
    {
        if (updateDepth != 0)
        {
            pendingChange = true;
            return;
        }
        Changed?.Invoke(this, EventArgs.Empty);
    }

    internal IDisposable BeginUpdate()
    {
        updateDepth++;
        return new UpdateScope(this);
    }

    private sealed class UpdateScope(BlueprintGraphDocument document) : IDisposable
    {
        private BlueprintGraphDocument? owner = document;

        public void Dispose()
        {
            BlueprintGraphDocument? current = owner;
            owner = null;
            if (current is null || --current.updateDepth != 0 || !current.pendingChange)
                return;
            current.pendingChange = false;
            current.publishChanged();
        }
    }
}
