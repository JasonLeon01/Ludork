using Avalonia;
using Ludork.Models;
using Ludork.Services;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.ViewModels.BlueprintGraph;

public sealed class BlueprintGraphEditorViewModel : NodifyEditorViewModelBase
{
    private static BlueprintGraphClipboard? clipboard;
    private readonly BlueprintGraphDocument document;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly IReadOnlyList<BlueprintGraphNodeDefinition> definitions;
    private readonly Dictionary<Guid, BlueprintGraphNodeViewModel> nodesById = [];
    private readonly Dictionary<Guid, BlueprintGraphPortViewModel> portsById = [];
    private readonly Dictionary<Guid, BlueprintGraphConnectionViewModel> connectionsById = [];

    public BlueprintGraphEditorViewModel(
        BlueprintGraphDocument document,
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        BlueprintVariableFieldBuilder fieldBuilder,
        bool isReadOnly,
        LuaMetadataTypeSnapshot? types)
    {
        this.document = document;
        this.definitions = definitions;
        this.fieldBuilder = fieldBuilder;
        document.IsTypeAssignable = types is null ? fieldBuilder.IsTypeAssignable : types.IsTypeAssignable;
        IsReadOnly = isReadOnly;
        BlueprintPendingConnection = new BlueprintPendingConnectionViewModel(this);
        PendingConnection = BlueprintPendingConnection;
    }

    internal async Task InitializeAsync(Func<CancellationToken, Task> prepareConnections,
        CancellationToken cancellationToken)
    {
        EditorUiBatch batch = new();
        int count = 0;
        foreach (BlueprintGraphNode node in document.Nodes)
        {
            cancellationToken.ThrowIfCancellationRequested();
            foreach (BlueprintGraphPort port in node.Inputs.Concat(node.Outputs))
                port.SetEnumResolver(fieldBuilder.ReadEnum);
            addNodeViewModel(node);
            if (++count % 16 == 0)
            {
                await EditorUiBatch.YieldAsync(cancellationToken);
                batch = new EditorUiBatch();
            }
            else
                await batch.YieldIfNeededAsync(cancellationToken);
        }
        await prepareConnections(cancellationToken);
        batch = new EditorUiBatch();
        count = 0;
        foreach (BlueprintGraphConnection connection in document.Connections)
        {
            cancellationToken.ThrowIfCancellationRequested();
            addConnectionViewModel(connection);
            if (++count % 64 == 0)
            {
                await EditorUiBatch.YieldAsync(cancellationToken);
                batch = new EditorUiBatch();
            }
            else
                await batch.YieldIfNeededAsync(cancellationToken);
        }
    }

    public BlueprintGraphDocument Document => document;
    public IReadOnlyList<BlueprintGraphNodeDefinition> Definitions => definitions;
    public BlueprintPendingConnectionViewModel BlueprintPendingConnection { get; }
    public event EventHandler? ParameterEdited;
    public event EventHandler? InputDraftChanged;
    public bool IsReadOnly { get; private set; }
    public bool CanPaste => !IsReadOnly && clipboard is not null && clipboard.Nodes.Count != 0;
    public bool CanCopySelected => SelectedNodes
        .OfType<BlueprintGraphNodeViewModel>()
        .Any(node => !node.Model.IsVirtual);
    public bool CanDeleteSelected => !IsReadOnly && CanCopySelected;

    internal void SynchronizeParameters(int index, JsonArray parameters)
    {
        BlueprintGraphNode? node = document.FindSavedNode(index);
        if (node is null)
            return;
        node.Parameters.Clear();
        foreach (JsonNode? parameter in parameters)
            node.Parameters.Add(parameter?.DeepClone());
        foreach (BlueprintGraphPort port in node.Inputs)
            if (port.ParameterIndex is int parameterIndex && parameterIndex < parameters.Count
                && portsById.TryGetValue(port.Id, out BlueprintGraphPortViewModel? view))
                view.SynchronizeValue(parameters[parameterIndex]);
    }

    internal IReadOnlyList<string> GetInputErrors()
    {
        return document.Nodes.SelectMany((node, index) => node.Inputs
            .Where(port => portsById.TryGetValue(port.Id, out BlueprintGraphPortViewModel? view) && view.HasInputError)
            .Select(port => $"{document.EventName} / {index + 1}: {node.Title} / {port.Name}: {portsById[port.Id].TextDraft!.Error}"))
            .ToArray();
    }

    internal bool CanTogglePlainTextInputs(BlueprintGraphNodeViewModel? node)
    {
        return !IsReadOnly && node is not null && !node.Model.IsVirtual
            && node.Model.Inputs.Any(port => port.SupportsEditor)
            && !node.Input.OfType<BlueprintGraphPortViewModel>().Any(port => port.HasInputError);
    }

    internal void TogglePlainTextInputs(BlueprintGraphNodeViewModel node)
    {
        if (!CanTogglePlainTextInputs(node))
            return;
        node.UsePlainTextInputs = !node.UsePlainTextInputs;
        foreach (BlueprintGraphPortViewModel port in node.Input.OfType<BlueprintGraphPortViewModel>())
            port.SetPlainTextInputs(node.UsePlainTextInputs);
        InputDraftChanged?.Invoke(this, EventArgs.Empty);
    }

    internal IReadOnlyList<BlueprintNodeInputState> CaptureInputStates()
    {
        if (!nodesById.Values.Any(node => node.UsePlainTextInputs))
            return [];
        JsonArray savedNodes = (JsonArray)BlueprintGraphCodec.Save(document).EventGraph["nodes"]!;
        return document.Nodes.Where(node => !node.IsVirtual)
            .Select((node, index) => (Node: nodesById[node.Id], Index: index))
            .Where(entry => entry.Node.UsePlainTextInputs && savedNodes[entry.Index] is JsonObject)
            .Select(entry => new BlueprintNodeInputState(entry.Index, savedNodes.Count,
                (JsonObject)savedNodes[entry.Index]!.DeepClone(),
                entry.Node.Input.OfType<BlueprintGraphPortViewModel>()
                    .Where(port => port.TextDraft is not null)
                    .ToDictionary(port => port.Model.Name,
                        port => port.TextDraft! with { ParameterIndex = port.Model.ParameterIndex }, StringComparer.Ordinal)))
            .ToArray();
    }

    internal void RestoreInputStates(IReadOnlyList<BlueprintNodeInputState> states)
    {
        if (states.Count == 0)
            return;
        BlueprintGraphNode[] nodes = document.Nodes.Where(node => !node.IsVirtual).ToArray();
        JsonArray savedNodes = (JsonArray)BlueprintGraphCodec.Save(document).EventGraph["nodes"]!;
        foreach ((BlueprintNodeInputState state, int index) in BlueprintNodeInputState.Match(states, savedNodes))
        {
            BlueprintGraphNodeViewModel node = nodesById[nodes[index].Id];
            node.UsePlainTextInputs = true;
            foreach (BlueprintGraphPortViewModel port in node.Input.OfType<BlueprintGraphPortViewModel>())
            {
                state.Drafts.TryGetValue(port.Model.Name, out BlueprintParameterTextDraft? draft);
                port.SetPlainTextInputs(true, draft);
            }
        }
    }

    internal void SelectAllNodes()
    {
        SelectedNodes.Clear();
        foreach (BlueprintGraphNodeViewModel node in Nodes
            .OfType<BlueprintGraphNodeViewModel>()
            .Where(node => !node.Model.IsVirtual))
        {
            SelectedNodes.Add(node);
        }
    }

    internal bool NudgeSelected(Vector delta)
    {
        if (IsReadOnly)
            return false;
        BlueprintGraphNodeViewModel[] selected = getSelectedRegularNodes();
        if (selected.Length == 0)
            return false;
        using IDisposable update = document.BeginUpdate();
        foreach (BlueprintGraphNodeViewModel node in selected)
        {
            node.Location = new Point(
                node.Location.X + delta.X,
                node.Location.Y + delta.Y);
        }
        return true;
    }

    public void SetReadOnly(bool value)
    {
        if (IsReadOnly == value)
            return;
        IsReadOnly = value;
        foreach (BlueprintGraphNodeViewModel node in Nodes.OfType<BlueprintGraphNodeViewModel>())
            node.RefreshReadOnly();
        foreach (BlueprintGraphPortViewModel port in portsById.Values)
            port.SetReadOnly(value);
    }

    public override void Connect(ConnectorViewModelBase first, ConnectorViewModelBase second)
    {
        if (IsReadOnly
            || first is not BlueprintGraphPortViewModel firstPort
            || second is not BlueprintGraphPortViewModel secondPort
            || firstPort.Model.NodeId == secondPort.Model.NodeId)
        {
            return;
        }
        BlueprintGraphPortViewModel source = firstPort.Model.Direction == BlueprintGraphPortDirection.Output
            ? firstPort
            : secondPort;
        BlueprintGraphPortViewModel target = ReferenceEquals(source, firstPort) ? secondPort : firstPort;
        if (source.Model.Direction != BlueprintGraphPortDirection.Output
            || target.Model.Direction != BlueprintGraphPortDirection.Input
            || !document.ArePortTypesCompatible(source.Model, target.Model))
        {
            return;
        }
        using IDisposable update = document.BeginUpdate();
        BlueprintGraphPort exclusive = BlueprintGraphDocument.GetExclusivePin(source.Model, target.Model);
        foreach (BlueprintGraphConnectionViewModel existing in getPortConnections(exclusive.Id).ToArray())
        {
            removeConnection(existing);
        }
        BlueprintGraphNode? sourceNode = document.FindNode(source.Model.NodeId);
        BlueprintGraphNode? targetNode = document.FindNode(target.Model.NodeId);
        if (sourceNode is null || targetNode is null)
            return;
        BlueprintGraphConnection connection = new(
            Guid.NewGuid(),
            null,
            createEndpoint(sourceNode, source.Model),
            createEndpoint(targetNode),
            source.Model.Id,
            target.Model.Id,
            source.Model.Kind,
            source.Model.ExternalKey is null ? source.Model.PinIndex : 0,
            target.Model.PinIndex,
            [],
            sourceNode.IsEntry && source.Model.Kind == BlueprintGraphPortKind.Exec);
        if (document.AddConnection(connection))
            addConnectionViewModel(connection);
    }

    public override void DisconnectConnector(ConnectorViewModelBase connector)
    {
        if (IsReadOnly || connector is not BlueprintGraphPortViewModel port)
            return;
        using IDisposable update = document.BeginUpdate();
        foreach (BlueprintGraphConnectionViewModel connection in getPortConnections(port.Model.Id).ToArray())
        {
            removeConnection(connection);
        }
    }

    public BlueprintGraphNodeViewModel AddNode(BlueprintGraphNodeDefinition definition, Point location)
    {
        BlueprintGraphNodeViewModel viewModel = addNode(definition, location);
        document.NotifyChanged();
        return viewModel;
    }

    public bool ContainsPort(BlueprintGraphPortViewModel port) =>
        portsById.TryGetValue(port.Model.Id, out BlueprintGraphPortViewModel? current) && ReferenceEquals(current, port);

    public BlueprintGraphNodeViewModel? AddNodeAndConnect(BlueprintNodePickerSelection selection,
        BlueprintGraphPortViewModel source, Point location)
    {
        if (IsReadOnly || !ContainsPort(source) || source.Model.Direction != BlueprintGraphPortDirection.Output
            || !definitions.Contains(selection.Definition))
            return null;
        BlueprintGraphPortDefinition? targetDefinition = selection.Definition.Ports.FirstOrDefault(port =>
            port.Direction == BlueprintGraphPortDirection.Input && port.Kind == source.Model.Kind
            && port.PinIndex == selection.TargetPinIndex);
        if (targetDefinition is null || source.Model.Kind == BlueprintGraphPortKind.Params
            && document.IsTypeAssignable?.Invoke(source.Model.TypeName, targetDefinition.TypeName) != true)
            return null;
        using IDisposable update = document.BeginUpdate();
        BlueprintGraphNodeViewModel node = addNode(selection.Definition, location);
        BlueprintGraphPortViewModel target = node.Input.OfType<BlueprintGraphPortViewModel>().First(port =>
            port.Model.Kind == source.Model.Kind && port.Model.PinIndex == targetDefinition.PinIndex);
        Connect(source, target);
        SelectedNodes.Clear();
        SelectedNodes.Add(node);
        return node;
    }

    public void DeleteSelected()
    {
        if (IsReadOnly)
            return;
        BlueprintGraphNodeViewModel[] selected = getSelectedRegularNodes();
        if (selected.Length == 0)
            return;
        using IDisposable update = document.BeginUpdate();
        HashSet<Guid> ids = selected.Select(node => node.Model.Id).ToHashSet();
        foreach (BlueprintGraphConnectionViewModel connection in selected
            .SelectMany(node => node.Model.Inputs.Concat(node.Model.Outputs))
            .SelectMany(port => getPortConnections(port.Id)).DistinctBy(connection => connection.Model.Id).ToArray())
        {
            removeConnection(connection);
        }
        if (document.Start?.NodeId is Guid startId && ids.Contains(startId))
            document.Start = null;
        foreach (BlueprintGraphNodeViewModel node in selected)
        {
            Nodes.Remove(node);
            document.Nodes.Remove(node.Model);
            nodesById.Remove(node.Model.Id);
            foreach (BlueprintGraphPortViewModel port in node.Input
                .Concat(node.Output)
                .OfType<BlueprintGraphPortViewModel>())
            {
                portsById.Remove(port.Model.Id);
                port.Dispose();
            }
            node.Dispose();
        }
        SelectedNodes.Clear();
        document.NotifyChanged();
    }

    public bool CanDisconnectNode(BlueprintGraphNodeViewModel? node, BlueprintGraphPortDirection direction)
    {
        if (IsReadOnly || node is null)
            return false;
        HashSet<Guid> portIds = getNodePortIds(node, direction);
        return portIds.Any(id => document.GetPortConnections(id).Any());
    }

    public void DisconnectNode(BlueprintGraphNodeViewModel node, BlueprintGraphPortDirection direction)
    {
        if (IsReadOnly)
            return;
        using IDisposable update = document.BeginUpdate();
        HashSet<Guid> portIds = getNodePortIds(node, direction);
        foreach (BlueprintGraphConnectionViewModel connection in portIds.SelectMany(getPortConnections)
            .DistinctBy(connection => connection.Model.Id).ToArray())
        {
            removeConnection(connection);
        }
    }

    private IEnumerable<BlueprintGraphConnectionViewModel> getPortConnections(Guid portId)
    {
        return document.GetPortConnections(portId)
            .Select(connection => connectionsById.GetValueOrDefault(connection.Id))
            .OfType<BlueprintGraphConnectionViewModel>();
    }

    private static HashSet<Guid> getNodePortIds(
        BlueprintGraphNodeViewModel node,
        BlueprintGraphPortDirection direction)
    {
        return (direction == BlueprintGraphPortDirection.Input ? node.Input : node.Output)
            .OfType<BlueprintGraphPortViewModel>()
            .Select(port => port.Model.Id)
            .ToHashSet();
    }

    public void CopySelected()
    {
        BlueprintGraphClipboard? copied = createClipboard(getSelectedRegularNodes());
        if (copied is not null)
            clipboard = copied;
    }

    internal void DuplicateSelected(Point location)
    {
        if (IsReadOnly)
            return;
        BlueprintGraphClipboard? copied = createClipboard(getSelectedRegularNodes());
        if (copied is not null)
            paste(copied, location);
    }

    public void Paste(Point location)
    {
        BlueprintGraphClipboard? copied = clipboard;
        if (IsReadOnly || copied is null || copied.Nodes.Count == 0)
            return;
        paste(copied, location);
    }

    private BlueprintGraphClipboard? createClipboard(
        IReadOnlyList<BlueprintGraphNodeViewModel> selected)
    {
        if (selected.Count == 0)
            return null;
        Dictionary<Guid, int> indices = selected
            .Select((node, index) => (node.Model.Id, index))
            .ToDictionary(entry => entry.Id, entry => entry.index);
        List<BlueprintGraphClipboardNode> copiedNodes = selected
            .Select(node => BlueprintGraphClipboardNode.FromModel(node.Model))
            .ToList();
        List<BlueprintGraphClipboardConnection> copiedConnections = [];
        foreach (BlueprintGraphConnection connection in document.Connections)
        {
            if (connection.Source.NodeId is not Guid sourceId
                || connection.Target.NodeId is not Guid targetId
                || !indices.TryGetValue(sourceId, out int sourceIndex)
                || !indices.TryGetValue(targetId, out int targetIndex))
            {
                continue;
            }
            copiedConnections.Add(new BlueprintGraphClipboardConnection(
                sourceIndex,
                targetIndex,
                connection.SourcePinIndex,
                connection.TargetPinIndex,
                connection.Kind,
                connection.RawData.DeepClone() as JsonObject ?? []));
        }
        return new BlueprintGraphClipboard(copiedNodes, copiedConnections);
    }

    private void paste(BlueprintGraphClipboard copied, Point location)
    {
        using IDisposable update = document.BeginUpdate();
        double minimumX = copied.Nodes.Min(node => node.X);
        double minimumY = copied.Nodes.Min(node => node.Y);
        List<BlueprintGraphNode> pasted = [];
        foreach (BlueprintGraphClipboardNode copiedNode in copied.Nodes)
        {
            Point nextLocation = new(
                location.X + copiedNode.X - minimumX,
                location.Y + copiedNode.Y - minimumY);
            BlueprintGraphNode node = createNode(
                copiedNode.Definition,
                copiedNode.RawData,
                copiedNode.Parameters,
                nextLocation,
                copiedNode.IsResolved,
                copiedNode.Title,
                copiedNode.Description);
            document.Nodes.Add(node);
            addNodeViewModel(node);
            pasted.Add(node);
        }
        foreach (BlueprintGraphClipboardConnection copiedConnection in copied.Connections)
        {
            BlueprintGraphNode sourceNode = pasted[copiedConnection.SourceIndex];
            BlueprintGraphNode targetNode = pasted[copiedConnection.TargetIndex];
            BlueprintGraphPort? sourcePort = sourceNode.FindPort(
                BlueprintGraphPortDirection.Output,
                copiedConnection.Kind,
                copiedConnection.SourcePinIndex);
            BlueprintGraphPort? targetPort = targetNode.FindPort(
                BlueprintGraphPortDirection.Input,
                copiedConnection.Kind,
                copiedConnection.TargetPinIndex);
            if (sourcePort is null || targetPort is null)
                continue;
            BlueprintGraphConnection connection = new(
                Guid.NewGuid(),
                null,
                BlueprintGraphEndpoint.Node(sourceNode.Id),
                BlueprintGraphEndpoint.Node(targetNode.Id),
                sourcePort.Id,
                targetPort.Id,
                copiedConnection.Kind,
                copiedConnection.SourcePinIndex,
                copiedConnection.TargetPinIndex,
                copiedConnection.RawData);
            if (document.ArePortTypesCompatible(sourcePort, targetPort))
            {
                if (document.AddConnection(connection))
                    addConnectionViewModel(connection);
            }
            else
            {
                document.AddLoadedConnection(connection, sourcePort, targetPort);
                addConnectionViewModel(connection);
            }
        }
        SelectedNodes.Clear();
        foreach (BlueprintGraphNode node in pasted)
            SelectedNodes.Add(nodesById[node.Id]);
        document.NotifyChanged();
    }

    private BlueprintGraphNodeViewModel[] getSelectedRegularNodes()
    {
        return SelectedNodes
            .OfType<BlueprintGraphNodeViewModel>()
            .Where(node => !node.Model.IsVirtual)
            .ToArray();
    }

    public void OrganizeLayout()
    {
        if (IsReadOnly)
            return;
        IReadOnlyDictionary<Guid, Point> positions = BlueprintGraphLayout.Compute(document);
        foreach (BlueprintGraphNodeViewModel node in Nodes.OfType<BlueprintGraphNodeViewModel>())
        {
            if (positions.TryGetValue(node.Model.Id, out Point position))
                node.SetLayoutLocation(position);
        }
        document.NotifyChanged();
    }

    internal void RemoveConnection(BlueprintGraphConnectionViewModel connection)
    {
        if (IsReadOnly)
            return;
        removeConnection(connection);
    }

    private BlueprintGraphNodeViewModel addNodeViewModel(BlueprintGraphNode node)
    {
        BlueprintGraphNodeViewModel viewModel = new(node, document, () => IsReadOnly);
        BlueprintGraphPortViewModel? findInput(string name)
        {
            return viewModel.Input
                .OfType<BlueprintGraphPortViewModel>()
                .FirstOrDefault(port => string.Equals(port.Model.Name, name, StringComparison.Ordinal));
        }
        JsonNode? getRawInputValue(string name)
        {
            return findInput(name)?.Model.Value?.DeepClone();
        }
        void setRawInputValue(string name, JsonNode? value)
        {
            findInput(name)?.ApplyExternalValue(value);
        }
        foreach (BlueprintGraphPort port in node.Inputs)
        {
            BlueprintGraphPortViewModel portViewModel = new(
                port,
                fieldBuilder,
                getRawInputValue,
                setRawInputValue,
                document,
                () => IsReadOnly);
            viewModel.Input.Add(portViewModel);
            portsById[port.Id] = portViewModel;
        }
        foreach (BlueprintGraphPort port in node.Outputs)
        {
            BlueprintGraphPortViewModel portViewModel = new(
                port,
                fieldBuilder,
                getRawInputValue,
                setRawInputValue,
                document,
                () => IsReadOnly);
            viewModel.Output.Add(portViewModel);
            portsById[port.Id] = portViewModel;
        }
        foreach (BlueprintGraphPortViewModel port in viewModel.Input.OfType<BlueprintGraphPortViewModel>())
        {
            port.ParameterValueChanged += (_, _) => synchronizeNodeParameters(node.Id);
            port.ParameterEdited += (_, _) => ParameterEdited?.Invoke(this, EventArgs.Empty);
            port.InputDraftChanged += (_, _) => InputDraftChanged?.Invoke(this, EventArgs.Empty);
        }
        Nodes.Add(viewModel);
        nodesById[node.Id] = viewModel;
        synchronizeNodeParameters(node.Id);
        return viewModel;
    }

    private BlueprintGraphNodeViewModel addNode(
        BlueprintGraphNodeDefinition definition,
        Point location)
    {
        JsonArray parameters = createInitialParameters(definition);
        JsonObject rawData = new()
        {
            ["nodeFunction"] = definition.RuntimePath,
            ["params"] = parameters.DeepClone(),
            ["pos"] = new JsonArray(location.X, location.Y),
        };
        BlueprintGraphNode node = createNode(definition, rawData, parameters, location, true);
        document.Nodes.Add(node);
        return addNodeViewModel(node);
    }

    private void synchronizeNodeParameters(Guid nodeId)
    {
        if (!nodesById.TryGetValue(nodeId, out BlueprintGraphNodeViewModel? node))
            return;
        BlueprintGraphPortViewModel[] inputs = node.Input
            .OfType<BlueprintGraphPortViewModel>()
            .Where(port => port.Model.Kind == BlueprintGraphPortKind.Params)
            .ToArray();
        foreach (BlueprintGraphPortViewModel input in inputs)
            input.SynchronizeDependencies(inputs);
    }

    private void addConnectionViewModel(BlueprintGraphConnection connection)
    {
        if (!portsById.TryGetValue(connection.SourcePortId, out BlueprintGraphPortViewModel? source)
            || !portsById.TryGetValue(connection.TargetPortId, out BlueprintGraphPortViewModel? target))
        {
            return;
        }
        BlueprintGraphConnectionViewModel viewModel = new(this, connection, source, target);
        connectionsById[connection.Id] = viewModel;
        Connections.Add(viewModel);
    }

    private void removeConnection(BlueprintGraphConnectionViewModel connection)
    {
        if (document.RemoveConnection(connection.Model.Id))
        {
            connectionsById.Remove(connection.Model.Id);
            Connections.Remove(connection);
        }
    }

    private BlueprintGraphNode createNode(
        BlueprintGraphNodeDefinition definition,
        JsonObject rawData,
        JsonArray parameters,
        Point location,
        bool isResolved,
        string? title = null,
        string? description = null)
    {
        Guid nodeId = Guid.NewGuid();
        JsonObject raw = rawData.DeepClone() as JsonObject ?? [];
        raw["nodeFunction"] = definition.RuntimePath;
        raw["params"] = parameters.DeepClone();
        raw["pos"] = new JsonArray(location.X, location.Y);
        BlueprintGraphNode node = new(
            nodeId,
            null,
            definition.RuntimePath,
            title ?? BlueprintNodeDisplayText.GetTitle(definition),
            location.X,
            location.Y,
            isResolved,
            false,
            null,
            raw,
            parameters,
            description);
        foreach (BlueprintGraphPortDefinition portDefinition in definition.Ports)
        {
            JsonNode? value = portDefinition.ParameterIndex is int parameterIndex
                && parameterIndex >= 0
                && parameterIndex < parameters.Count
                ? parameters[parameterIndex]
                : portDefinition.DefaultValue;
            BlueprintGraphPort port = new(
                Guid.NewGuid(),
                nodeId,
                portDefinition.Name,
                portDefinition.Kind,
                portDefinition.Direction,
                portDefinition.PinIndex,
                portDefinition.TypeName,
                portDefinition.ParameterIndex,
                portDefinition.SupportsEditor,
                value,
                portDefinition.Meta,
                portDefinition.ResolveEnum);
            node.AddPort(port);
        }
        return node;
    }

    private static JsonArray createInitialParameters(BlueprintGraphNodeDefinition definition)
    {
        JsonArray parameters = [];
        foreach (BlueprintGraphPortDefinition port in definition.Ports
            .Where(port => port.Direction == BlueprintGraphPortDirection.Input
                && port.Kind == BlueprintGraphPortKind.Params
                && port.ParameterIndex is not null)
            .OrderBy(port => port.ParameterIndex))
        {
            int index = port.ParameterIndex!.Value;
            while (parameters.Count <= index)
                parameters.Add(null);
            parameters[index] = port.DefaultValue?.DeepClone();
        }
        return parameters;
    }

    private static BlueprintGraphEndpoint createEndpoint(BlueprintGraphNode node, BlueprintGraphPort? port = null)
    {
        if (port?.ExternalKey is string key)
            return BlueprintGraphEndpoint.External(key, node.Id);
        return node.IsVirtual && node.ExternalKey is not null
            ? BlueprintGraphEndpoint.External(node.ExternalKey, node.Id)
            : BlueprintGraphEndpoint.Node(node.Id);
    }
}
