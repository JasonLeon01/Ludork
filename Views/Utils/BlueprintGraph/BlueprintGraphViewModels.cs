using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using CommunityToolkit.Mvvm.Input;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintGraphEditorViewModel : NodifyEditorViewModelBase
{
    private static BlueprintGraphClipboard? clipboard;
    private readonly GameDataService gameData;
    private readonly BlueprintGraphDocument document;
    private readonly BlueprintVariableFieldBuilder fieldBuilder;
    private readonly BlueprintNodeParameterEditorFactory parameterEditorFactory;
    private readonly string assetsDirectory;
    private readonly int cellSize;
    private readonly IReadOnlyList<BlueprintGraphNodeDefinition> definitions;
    private readonly Dictionary<Guid, BlueprintGraphNodeViewModel> nodesById = [];
    private readonly Dictionary<Guid, BlueprintGraphPortViewModel> portsById = [];

    public BlueprintGraphEditorViewModel(
        GameDataService gameData,
        BlueprintGraphDocument document,
        IReadOnlyList<BlueprintGraphNodeDefinition> definitions,
        BlueprintVariableFieldBuilder fieldBuilder,
        BlueprintNodeParameterEditorFactory parameterEditorFactory,
        string assetsDirectory,
        int cellSize,
        bool isReadOnly)
    {
        this.gameData = gameData;
        this.document = document;
        this.definitions = definitions;
        this.fieldBuilder = fieldBuilder;
        this.parameterEditorFactory = parameterEditorFactory;
        this.assetsDirectory = assetsDirectory;
        this.cellSize = cellSize;
        IsReadOnly = isReadOnly;
        BlueprintPendingConnection = new BlueprintPendingConnectionViewModel(this);
        PendingConnection = BlueprintPendingConnection;
        foreach (BlueprintGraphNode node in document.Nodes)
            addNodeViewModel(node);
        foreach (BlueprintGraphConnection connection in document.Connections)
            addConnectionViewModel(connection);
    }

    public BlueprintGraphDocument Document => document;
    public IReadOnlyList<BlueprintGraphNodeDefinition> Definitions => definitions;
    public BlueprintPendingConnectionViewModel BlueprintPendingConnection { get; }
    public event EventHandler? ParameterEdited;
    public bool IsReadOnly { get; private set; }
    public bool CanPaste => !IsReadOnly && clipboard is not null && clipboard.Nodes.Count != 0;
    public bool CanCopySelected => SelectedNodes
        .OfType<BlueprintGraphNodeViewModel>()
        .Any(node => !node.Model.IsVirtual);
    public bool CanDeleteSelected => !IsReadOnly && CanCopySelected;

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
            || source.Model.Kind != target.Model.Kind)
        {
            return;
        }
        foreach (BlueprintGraphConnectionViewModel existing in Connections
            .OfType<BlueprintGraphConnectionViewModel>()
            .Where(connection => connection.Model.TargetPortId == target.Model.Id)
            .ToArray())
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
            createEndpoint(sourceNode),
            createEndpoint(targetNode),
            source.Model.Id,
            target.Model.Id,
            source.Model.Kind,
            source.Model.PinIndex,
            target.Model.PinIndex,
            []);
        if (document.AddConnection(connection))
            addConnectionViewModel(connection);
    }

    public override void DisconnectConnector(ConnectorViewModelBase connector)
    {
        if (IsReadOnly)
            return;
        foreach (BlueprintGraphConnectionViewModel connection in Connections
            .OfType<BlueprintGraphConnectionViewModel>()
            .Where(value => ReferenceEquals(value.Source, connector) || ReferenceEquals(value.Target, connector))
            .ToArray())
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

    public BlueprintGraphNodeViewModel? AddNodeAndConnect(
        BlueprintGraphNodeDefinition definition,
        Point location,
        BlueprintGraphPortViewModel source)
    {
        if (IsReadOnly)
            return null;
        BlueprintGraphNodeViewModel node = addNode(definition, location);
        BlueprintGraphPortViewModel[] candidates = node.Input
            .OfType<BlueprintGraphPortViewModel>()
            .Where(port => port.Model.Kind == source.Model.Kind)
            .OrderBy(port => port.Model.PinIndex)
            .ToArray();
        BlueprintGraphPortViewModel? target = source.Model.Kind == BlueprintGraphPortKind.Params
            ? candidates
                .Where(port => string.Equals(
                    port.Model.TypeName,
                    source.Model.TypeName,
                    StringComparison.Ordinal))
                .LastOrDefault() ?? candidates.FirstOrDefault()
            : candidates.FirstOrDefault();
        if (target is null)
        {
            removeNode(node);
            return null;
        }
        Connect(source, target);
        SelectedNodes.Clear();
        SelectedNodes.Add(node);
        document.NotifyChanged();
        return node;
    }

    public void DeleteSelected()
    {
        if (IsReadOnly)
            return;
        BlueprintGraphNodeViewModel[] selected = SelectedNodes
            .OfType<BlueprintGraphNodeViewModel>()
            .Where(node => !node.Model.IsVirtual)
            .ToArray();
        if (selected.Length == 0)
            return;
        HashSet<Guid> ids = selected.Select(node => node.Model.Id).ToHashSet();
        foreach (BlueprintGraphConnectionViewModel connection in Connections
            .OfType<BlueprintGraphConnectionViewModel>()
            .Where(value => ids.Contains(value.Model.Source.NodeId ?? Guid.Empty)
                || ids.Contains(value.Model.Target.NodeId ?? Guid.Empty))
            .ToArray())
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

    public bool CanSetAsStart(BlueprintGraphNodeViewModel? node)
    {
        return !IsReadOnly
            && node is not null
            && !node.Model.IsVirtual
            && !node.Model.IsStart
            && node.Model.Outputs.Any(port => port.Kind == BlueprintGraphPortKind.Exec);
    }

    public bool CanClearStart(BlueprintGraphNodeViewModel? node)
    {
        return !IsReadOnly && node is not null && !node.Model.IsVirtual && node.Model.IsStart;
    }

    public void SetAsStart(BlueprintGraphNodeViewModel node)
    {
        if (CanSetAsStart(node))
            document.Start = BlueprintGraphEndpoint.Node(node.Model.Id);
    }

    public void ClearStart(BlueprintGraphNodeViewModel node)
    {
        if (CanClearStart(node))
            document.Start = null;
    }

    public void CopySelected()
    {
        BlueprintGraphNodeViewModel[] selected = SelectedNodes
            .OfType<BlueprintGraphNodeViewModel>()
            .Where(node => !node.Model.IsVirtual)
            .ToArray();
        if (selected.Length == 0)
            return;
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
                connection.Kind));
        }
        clipboard = new BlueprintGraphClipboard(copiedNodes, copiedConnections);
    }

    public void Paste(Point location)
    {
        if (IsReadOnly || clipboard is null || clipboard.Nodes.Count == 0)
            return;
        double minimumX = clipboard.Nodes.Min(node => node.X);
        double minimumY = clipboard.Nodes.Min(node => node.Y);
        List<BlueprintGraphNode> pasted = [];
        foreach (BlueprintGraphClipboardNode copied in clipboard.Nodes)
        {
            Point nextLocation = new(
                location.X + copied.X - minimumX,
                location.Y + copied.Y - minimumY);
            BlueprintGraphNode node = createNode(
                copied.Definition,
                copied.RawData,
                copied.Parameters,
                nextLocation,
                copied.IsResolved);
            document.Nodes.Add(node);
            addNodeViewModel(node);
            pasted.Add(node);
        }
        foreach (BlueprintGraphClipboardConnection copied in clipboard.Connections)
        {
            BlueprintGraphNode sourceNode = pasted[copied.SourceIndex];
            BlueprintGraphNode targetNode = pasted[copied.TargetIndex];
            BlueprintGraphPort? sourcePort = sourceNode.FindPort(
                BlueprintGraphPortDirection.Output,
                copied.Kind,
                copied.SourcePinIndex);
            BlueprintGraphPort? targetPort = targetNode.FindPort(
                BlueprintGraphPortDirection.Input,
                copied.Kind,
                copied.TargetPinIndex);
            if (sourcePort is null || targetPort is null)
                continue;
            BlueprintGraphConnection connection = new(
                Guid.NewGuid(),
                null,
                BlueprintGraphEndpoint.Node(sourceNode.Id),
                BlueprintGraphEndpoint.Node(targetNode.Id),
                sourcePort.Id,
                targetPort.Id,
                copied.Kind,
                copied.SourcePinIndex,
                copied.TargetPinIndex,
                []);
            if (document.AddConnection(connection))
                addConnectionViewModel(connection);
        }
        SelectedNodes.Clear();
        foreach (BlueprintGraphNode node in pasted)
            SelectedNodes.Add(nodesById[node.Id]);
        document.NotifyChanged();
    }

    public void OrganizeLayout()
    {
        if (IsReadOnly)
            return;
        IReadOnlyDictionary<Guid, Point> positions = BlueprintGraphLayout.Compute(document);
        foreach (BlueprintGraphNodeViewModel node in Nodes.OfType<BlueprintGraphNodeViewModel>())
        {
            if (positions.TryGetValue(node.Model.Id, out Point position))
                node.Location = position;
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
                gameData,
                port,
                fieldBuilder,
                parameterEditorFactory,
                getRawInputValue,
                setRawInputValue,
                assetsDirectory,
                cellSize,
                document,
                () => IsReadOnly);
            viewModel.Input.Add(portViewModel);
            portsById[port.Id] = portViewModel;
        }
        foreach (BlueprintGraphPort port in node.Outputs)
        {
            BlueprintGraphPortViewModel portViewModel = new(
                gameData,
                port,
                fieldBuilder,
                parameterEditorFactory,
                getRawInputValue,
                setRawInputValue,
                assetsDirectory,
                cellSize,
                document,
                () => IsReadOnly);
            viewModel.Output.Add(portViewModel);
            portsById[port.Id] = portViewModel;
        }
        foreach (BlueprintGraphPortViewModel port in viewModel.Input.OfType<BlueprintGraphPortViewModel>())
        {
            port.ParameterValueChanged += (_, _) => synchronizeNodeParameters(node.Id);
            port.ParameterEdited += (_, _) => ParameterEdited?.Invoke(this, EventArgs.Empty);
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

    private void removeNode(BlueprintGraphNodeViewModel node)
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
        Connections.Add(new BlueprintGraphConnectionViewModel(this, connection, source, target));
    }

    private void removeConnection(BlueprintGraphConnectionViewModel connection)
    {
        if (document.RemoveConnection(connection.Model.Id))
            Connections.Remove(connection);
    }

    private BlueprintGraphNode createNode(
        BlueprintGraphNodeDefinition definition,
        JsonObject rawData,
        JsonArray parameters,
        Point location,
        bool isResolved)
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
            definition.Title,
            location.X,
            location.Y,
            isResolved,
            false,
            null,
            raw,
            parameters,
            definition.Description);
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
                portDefinition.Meta);
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

    private static BlueprintGraphEndpoint createEndpoint(BlueprintGraphNode node)
    {
        return node.IsVirtual && node.ExternalKey is not null
            ? BlueprintGraphEndpoint.External(node.ExternalKey, node.Id)
            : BlueprintGraphEndpoint.Node(node.Id);
    }
}

public sealed class BlueprintGraphNodeViewModel : NodeViewModelBase, IDisposable
{
    private readonly BlueprintGraphDocument document;
    private readonly Func<bool> isReadOnly;
    private bool restoringLocation;

    public BlueprintGraphNodeViewModel(
        BlueprintGraphNode model,
        BlueprintGraphDocument document,
        Func<bool> isReadOnly)
    {
        Model = model;
        this.document = document;
        this.isReadOnly = isReadOnly;
        Title = model.Title;
        Location = new Point(model.X, model.Y);
        PropertyChanged += onViewModelPropertyChanged;
        model.PropertyChanged += onModelPropertyChanged;
    }

    public BlueprintGraphNode Model { get; }
    public string StartMarker => Model.IsStart ? "S" : string.Empty;
    public bool IsUnresolved => !Model.IsResolved;
    public string ToolTip => Model.IsResolved
        ? string.IsNullOrWhiteSpace(Model.Description)
            ? Model.NodeFunction
            : $"{Model.NodeFunction}\n\n{Model.Description}"
        : string.IsNullOrWhiteSpace(Model.Description)
            ? $"{Model.NodeFunction}\n{LocaleService.Get("NODE_UNRESOLVED")}"
            : $"{Model.NodeFunction}\n\n{Model.Description}\n\n{LocaleService.Get("NODE_UNRESOLVED")}";
    public IBrush HeaderBrush => Model.IsVirtual
        ? new SolidColorBrush(Color.Parse("#3c6432"))
        : Model.IsResolved
            ? new SolidColorBrush(Color.Parse("#3b3b3b"))
            : new SolidColorBrush(Color.Parse("#713b3b"));

    public void Dispose()
    {
        PropertyChanged -= onViewModelPropertyChanged;
        Model.PropertyChanged -= onModelPropertyChanged;
    }

    public void RefreshReadOnly()
    {
        if (isReadOnly())
            restoreLocation();
    }

    private void onViewModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (!string.Equals(args.PropertyName, nameof(Location), StringComparison.Ordinal))
            return;
        if (isReadOnly())
        {
            restoreLocation();
            return;
        }
        Model.X = Location.X;
        Model.Y = Location.Y;
        document.NotifyChanged();
    }

    private void restoreLocation()
    {
        if (restoringLocation
            || Math.Abs(Location.X - Model.X) < double.Epsilon
                && Math.Abs(Location.Y - Model.Y) < double.Epsilon)
        {
            return;
        }
        restoringLocation = true;
        Location = new Point(Model.X, Model.Y);
        restoringLocation = false;
    }

    private void onModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (string.Equals(args.PropertyName, nameof(BlueprintGraphNode.IsStart), StringComparison.Ordinal))
            OnPropertyChanged(nameof(StartMarker));
    }
}

public sealed class BlueprintGraphPortViewModel : ConnectorViewModelBase, IDisposable
{
    private readonly BlueprintGraphDocument document;
    private readonly Func<bool> isReadOnly;
    private readonly string displayTypeName;
    private bool disposed;

    public BlueprintGraphPortViewModel(
        GameDataService gameData,
        BlueprintGraphPort model,
        BlueprintVariableFieldBuilder fieldBuilder,
        BlueprintNodeParameterEditorFactory parameterEditorFactory,
        Func<string, JsonNode?> getRawSiblingValue,
        Action<string, JsonNode?> setRawSiblingValue,
        string assetsDirectory,
        int cellSize,
        BlueprintGraphDocument document,
        Func<bool> isReadOnly)
    {
        Model = model;
        this.document = document;
        this.isReadOnly = isReadOnly;
        displayTypeName = model.TypeName;
        Title = EditorDisplayName.Format(model.Name);
        CanConnect = !isReadOnly();
        Flow = model.Direction == BlueprintGraphPortDirection.Input
            ? ConnectorFlow.Input
            : ConnectorFlow.Output;
        IsConnected = model.IsConnected;
        if (model.Direction == BlueprintGraphPortDirection.Input
            && model.Kind == BlueprintGraphPortKind.Params
            && model.SupportsEditor)
        {
            BlueprintVariableField field = fieldBuilder.BuildNodeParameter(model);
            displayTypeName = getDisplayTypeName(field, model.TypeName);
            BlueprintVariableForm form = new()
            {
                AssetsDirectory = assetsDirectory,
                ProjectDirectory = Path.GetDirectoryName(assetsDirectory) ?? string.Empty,
                CellSize = cellSize,
                HistoryGameData = gameData,
                IsReadOnly = isReadOnly(),
                ShowFieldNames = false,
                MinWidth = 180,
                MaxWidth = 280,
            };
            form.CustomValueEditorFactory = request => parameterEditorFactory.Create(
                request,
                getRawSiblingValue,
                setRawSiblingValue,
                () => !disposed);
            form.PointerPressed += onParameterEditorPointerPressed;
            form.SetFields([field]);
            form.ValueChanged += (_, args) =>
            {
                if (isReadOnly())
                    return;
                model.Value = args.Value?.DeepClone();
                document.NotifyChanged();
                ParameterValueChanged?.Invoke(this, EventArgs.Empty);
                ParameterEdited?.Invoke(this, EventArgs.Empty);
            };
            Editor = form;
            ParameterForm = form;
        }
        model.PropertyChanged += onModelPropertyChanged;
    }

    public BlueprintGraphPort Model { get; }
    public Control? Editor { get; }
    public BlueprintVariableForm? ParameterForm { get; }
    public event EventHandler? ParameterValueChanged;
    public event EventHandler? ParameterEdited;
    public string DisplayTitle => Model.Kind == BlueprintGraphPortKind.Params
        ? $"{Title} ({displayTypeName})"
        : Title;
    public bool IsEditorVisible => Model.IsEditorVisible;
    public IBrush Brush => Model.Kind == BlueprintGraphPortKind.Exec
        ? new SolidColorBrush(Color.Parse("#e6b84f"))
        : new SolidColorBrush(Color.Parse("#65ad67"));

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        Model.PropertyChanged -= onModelPropertyChanged;
    }

    public void ApplyExternalValue(JsonNode? value)
    {
        if (disposed || isReadOnly() || JsonNode.DeepEquals(Model.Value, value))
            return;
        Model.Value = value?.DeepClone();
        ParameterForm?.SetFieldValue(Model.Name, value);
        document.NotifyChanged();
        ParameterValueChanged?.Invoke(this, EventArgs.Empty);
        ParameterEdited?.Invoke(this, EventArgs.Empty);
    }

    public void SetReadOnly(bool value)
    {
        CanConnect = !value;
        if (ParameterForm is not null)
            ParameterForm.IsReadOnly = value;
    }

    public void SynchronizeDependencies(IEnumerable<BlueprintGraphPortViewModel> parameters)
    {
        if (ParameterForm is null)
            return;
        foreach (BlueprintGraphPortViewModel parameter in parameters)
        {
            JsonNode? value = parameter.Model.IsConnected ? null : parameter.Model.Value;
            ParameterForm.SetDependencyValue(parameter.Model.Name, value);
        }
    }

    private void onModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (string.Equals(args.PropertyName, nameof(BlueprintGraphPort.IsConnected), StringComparison.Ordinal))
        {
            IsConnected = Model.IsConnected;
            OnPropertyChanged(nameof(IsEditorVisible));
            ParameterValueChanged?.Invoke(this, EventArgs.Empty);
        }
        else if (string.Equals(args.PropertyName, nameof(BlueprintGraphPort.IsEditorVisible), StringComparison.Ordinal))
        {
            OnPropertyChanged(nameof(IsEditorVisible));
        }
    }

    private static string getDisplayTypeName(BlueprintVariableField field, string fallback)
    {
        return field.EditorKind switch
        {
            BlueprintVariableEditorKind.MoveRoute => "MoveRoute",
            BlueprintVariableEditorKind.TransferPosition => "TransferPos",
            BlueprintVariableEditorKind.BlueprintClass => "BlueprintClass",
            BlueprintVariableEditorKind.CommonFunction => "CommonFunction",
            _ => fallback,
        };
    }

    private static void onParameterEditorPointerPressed(
        object? sender,
        PointerPressedEventArgs args)
    {
        args.Handled = true;
    }
}

public sealed class BlueprintGraphConnectionViewModel : ConnectionViewModelBase
{
    private readonly BlueprintGraphEditorViewModel editor;

    public BlueprintGraphConnectionViewModel(
        BlueprintGraphEditorViewModel editor,
        BlueprintGraphConnection model,
        BlueprintGraphPortViewModel source,
        BlueprintGraphPortViewModel target) : base(editor, source, target)
    {
        this.editor = editor;
        Model = model;
    }

    public BlueprintGraphConnection Model { get; }
    public IBrush Stroke => Model.Kind == BlueprintGraphPortKind.Exec
        ? new SolidColorBrush(Color.Parse("#e6b84f"))
        : new SolidColorBrush(Color.Parse("#65ad67"));

    public override void DisconnectConnection(ConnectionViewModelBase connection)
    {
        editor.RemoveConnection(this);
    }
}

internal sealed class BlueprintGraphClipboard
{
    public BlueprintGraphClipboard(
        IReadOnlyList<BlueprintGraphClipboardNode> nodes,
        IReadOnlyList<BlueprintGraphClipboardConnection> connections)
    {
        Nodes = nodes;
        Connections = connections;
    }

    public IReadOnlyList<BlueprintGraphClipboardNode> Nodes { get; }
    public IReadOnlyList<BlueprintGraphClipboardConnection> Connections { get; }
}

internal sealed class BlueprintGraphClipboardNode
{
    private BlueprintGraphClipboardNode(
        BlueprintGraphNodeDefinition definition,
        JsonObject rawData,
        JsonArray parameters,
        double x,
        double y,
        bool isResolved)
    {
        Definition = definition;
        RawData = rawData;
        Parameters = parameters;
        X = x;
        Y = y;
        IsResolved = isResolved;
    }

    public BlueprintGraphNodeDefinition Definition { get; }
    public JsonObject RawData { get; }
    public JsonArray Parameters { get; }
    public double X { get; }
    public double Y { get; }
    public bool IsResolved { get; }

    public static BlueprintGraphClipboardNode FromModel(BlueprintGraphNode node)
    {
        IReadOnlyList<BlueprintGraphPortDefinition> ports = node.Inputs
            .Concat(node.Outputs)
            .Select(port => new BlueprintGraphPortDefinition(
                port.Name,
                port.Kind,
                port.Direction,
                port.PinIndex,
                port.TypeName,
                port.ParameterIndex,
                port.SupportsEditor,
                port.Value,
                port.Meta))
            .ToArray();
        BlueprintGraphNodeDefinition definition = new(
            node.NodeFunction,
            node.Title,
            ports,
            description: node.Description);
        JsonArray parameters = node.Parameters.DeepClone() as JsonArray ?? [];
        foreach (BlueprintGraphPort port in node.Inputs)
        {
            if (port.Kind != BlueprintGraphPortKind.Params
                || port.ParameterIndex is not int parameterIndex)
            {
                continue;
            }
            while (parameters.Count <= parameterIndex)
                parameters.Add(null);
            parameters[parameterIndex] = port.Value?.DeepClone();
        }
        return new BlueprintGraphClipboardNode(
            definition,
            node.RawData.DeepClone() as JsonObject ?? [],
            parameters,
            node.X,
            node.Y,
            node.IsResolved);
    }
}

internal sealed record BlueprintGraphClipboardConnection(
    int SourceIndex,
    int TargetIndex,
    int SourcePinIndex,
    int TargetPinIndex,
    BlueprintGraphPortKind Kind);

public sealed class BlueprintPendingConnectionViewModel : PendingConnectionViewModelBase
{
    private readonly BlueprintGraphEditorViewModel editor;

    public BlueprintPendingConnectionViewModel(BlueprintGraphEditorViewModel editor) : base(editor)
    {
        this.editor = editor;
        CompleteCommand = new RelayCommand<ConnectorViewModelBase?>(complete);
    }

    public RelayCommand<ConnectorViewModelBase?> CompleteCommand { get; }
    public event EventHandler<BlueprintConnectionDropEventArgs>? EmptyDropRequested;

    private void complete(ConnectorViewModelBase? target)
    {
        if (editor.IsReadOnly)
        {
            Source = null;
            return;
        }
        ConnectorViewModelBase? source = Source;
        if (source is null)
            return;
        if (target is null)
        {
            if (source is BlueprintGraphPortViewModel port
                && port.Model.Direction == BlueprintGraphPortDirection.Output)
            {
                EmptyDropRequested?.Invoke(this, new BlueprintConnectionDropEventArgs(port));
            }
            Source = null;
            return;
        }
        if (!ReferenceEquals(source, target))
            editor.Connect(source, target);
        Source = null;
    }
}

public sealed class BlueprintConnectionDropEventArgs : EventArgs
{
    public BlueprintConnectionDropEventArgs(BlueprintGraphPortViewModel source)
    {
        Source = source;
    }

    public BlueprintGraphPortViewModel Source { get; }
}
