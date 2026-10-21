using System;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Models;

public sealed class BlueprintGraphDocument
{
    private BlueprintGraphEndpoint? start;
    private int updateDepth;
    private bool pendingChange;

    public BlueprintGraphDocument(string eventName, JsonObject rawEventGraph)
    {
        EventName = eventName;
        RawEventGraph = (JsonObject)rawEventGraph.DeepClone();
    }

    public event EventHandler? Changed;

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
        BlueprintGraphConnection? connection = Connections.FirstOrDefault(value => value.Id == id);
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
            current.NotifyChanged();
        }
    }
}
