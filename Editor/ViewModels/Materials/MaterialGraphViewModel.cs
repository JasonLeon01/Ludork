using Avalonia;
using NodifyM.Avalonia.ViewModelBase;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels.Materials;

public sealed class MaterialGraphViewModel : NodifyEditorViewModelBase
{
    private readonly JsonObject asset;
    private readonly Dictionary<string, JsonObject> descriptors;
    private readonly Action changed;
    private readonly JsonObject defaults;
    private JsonArray parameterChoices = [];
    private readonly Dictionary<string, MaterialGraphNodeViewModel> byId = new(StringComparer.Ordinal);
    private static JsonArray? clipboard;

    public MaterialGraphViewModel(JsonObject asset, JsonObject registry, Action changed)
    {
        this.asset = asset;
        this.changed = changed;
        defaults = (JsonObject)registry["parameterDefaults"]!;
        descriptors = ((JsonArray)registry["nodes"]!).OfType<JsonObject>()
            .ToDictionary(node => node["type"]!.GetValue<string>(), StringComparer.Ordinal);
        MaterialPendingConnection = new MaterialGraphPendingConnectionViewModel(this);
        PendingConnection = MaterialPendingConnection;
        foreach (JsonObject node in GraphNodes.OfType<JsonObject>())
            addView(node);
        refreshConnections();
    }

    public MaterialGraphPendingConnectionViewModel MaterialPendingConnection { get; }
    public JsonArray GraphNodes => (JsonArray)asset["graph"]!["nodes"]!;
    public IReadOnlyCollection<JsonObject> NodeDescriptors => descriptors.Values;
    public MaterialGraphNodeViewModel? SelectedNode => SelectedNodes.OfType<MaterialGraphNodeViewModel>().FirstOrDefault();
    public event EventHandler? StructureChanged;

    public void AddNode(string type, Point position)
    {
        JsonObject descriptor = descriptors[type];
        JsonObject properties = new();
        foreach (JsonObject property in (descriptor["properties"] as JsonArray ?? []).OfType<JsonObject>())
            if (property["default"] is JsonNode value)
                properties[property["id"]!.GetValue<string>()] = value.DeepClone();
        string id = Guid.NewGuid().ToString("N");
        JsonObject node = new() { ["id"] = id, ["type"] = type, ["inputs"] = new JsonObject(), ["properties"] = properties };
        GraphNodes.Add(node);
        positions()[id] = new JsonArray(position.X, position.Y);
        MaterialGraphNodeViewModel view = addView(node);
        SelectedNodes.Clear();
        SelectedNodes.Add(view);
        changed();
        StructureChanged?.Invoke(this, EventArgs.Empty);
    }

    public override void Connect(ConnectorViewModelBase first, ConnectorViewModelBase second)
    {
        if (first is not MaterialGraphPortViewModel a || second is not MaterialGraphPortViewModel b || a.IsInput == b.IsInput)
            return;
        MaterialGraphPortViewModel source = a.IsInput ? b : a;
        MaterialGraphPortViewModel target = a.IsInput ? a : b;
        if (source.NodeId == target.NodeId || reaches(source.NodeId, target.NodeId, []))
            return;
        if ((source.ValueType == "texture" || target.ValueType == "texture")
            && source.ValueType != target.ValueType && source.ValueType != "parameter")
            return;
        JsonObject link = new() { ["node"] = source.NodeId, ["port"] = source.Id };
        if (JsonNode.DeepEquals(target.Value, link))
            return;
        writableInputs(byId[target.NodeId].Data)[target.Id] = link;
        refreshConnections();
        changed();
        StructureChanged?.Invoke(this, EventArgs.Empty);
    }

    public override void DisconnectConnector(ConnectorViewModelBase connector)
    {
        if (connector is not MaterialGraphPortViewModel port || !CanDisconnectPort(port))
            return;
        removeLinks((node, key, link) => port.IsInput ? node["id"]!.GetValue<string>() == port.NodeId && key == port.Id
            : link["node"]?.GetValue<string>() == port.NodeId && link["port"]?.GetValue<string>() == port.Id);
    }

    public bool CanDisconnectPort(MaterialGraphPortViewModel port) => port.IsInput ? port.HasLink
        : GraphNodes.OfType<JsonObject>().Any(node => inputs(node).Any(pair => isLink(pair.Value)
            && pair.Value!["node"]?.GetValue<string>() == port.NodeId && pair.Value["port"]?.GetValue<string>() == port.Id));

    public bool CanDisconnectNode(MaterialGraphNodeViewModel node, bool input) => (input ? node.Input : node.Output)
        .OfType<MaterialGraphPortViewModel>().Any(CanDisconnectPort);

    public void DisconnectNode(MaterialGraphNodeViewModel node, bool input)
    {
        removeLinks((owner, _, link) => input ? owner["id"]!.GetValue<string>() == node.Id : link["node"]?.GetValue<string>() == node.Id);
    }

    public bool CanSetConstant(MaterialGraphPortViewModel port) => port.IsInput && !port.HasLink && !port.HasLiteral;

    public void SetConstant(MaterialGraphPortViewModel port)
    {
        if (!CanSetConstant(port))
            return;
        writableInputs(port.Node.Data)[port.Id] = port.Descriptor["default"]?.DeepClone() ?? DefaultValue(port.ValueType);
        port.RefreshInline();
        changed();
    }

    public bool CanResetInput(MaterialGraphPortViewModel port) => port.IsInput && port.HasLiteral;

    public void ResetInput(MaterialGraphPortViewModel port)
    {
        if (!CanResetInput(port))
            return;
        inputs(port.Node.Data).Remove(port.Id);
        port.RefreshInline();
        changed();
    }

    public void SetInput(MaterialGraphPortViewModel port, JsonNode? value)
    {
        if (!port.IsInput || JsonNode.DeepEquals(port.Value, value))
            return;
        writableInputs(port.Node.Data)[port.Id] = value?.DeepClone();
        changed();
    }

    public void SetProperty(MaterialGraphNodeViewModel node, string id, JsonNode? value)
    {
        if (JsonNode.DeepEquals(node.Data["properties"]?[id], value))
            return;
        if (node.Data["properties"] is not JsonObject properties)
            node.Data["properties"] = properties = new JsonObject();
        properties[id] = value?.DeepClone();
        RefreshParameterNames();
        changed();
    }

    public JsonNode? DefaultValue(string type) => defaults[type == "numeric" ? "float" : type]?.DeepClone();

    public JsonObject PropertyDescriptor(MaterialGraphNodeViewModel node, JsonObject original)
    {
        JsonObject descriptor = (JsonObject)original.DeepClone();
        if (node.Type == "Parameter" && descriptor["id"]?.GetValue<string>() == "id")
            descriptor["options"] = parameterChoices.DeepClone();
        return descriptor;
    }

    private static bool isLink(JsonNode? value) => value is JsonObject link && link["node"] is JsonValue && link["port"] is JsonValue;

    private void removeLinks(Func<JsonObject, string, JsonObject, bool> matches)
    {
        bool removed = false;
        foreach (JsonObject node in GraphNodes.OfType<JsonObject>())
        {
            JsonObject values = inputs(node);
            foreach ((string key, JsonNode? value) in values.ToArray())
                if (isLink(value) && matches(node, key, (JsonObject)value!))
                    removed |= values.Remove(key);
        }
        if (removed)
            completeLinkChange();
    }

    private void completeLinkChange()
    {
        refreshConnections();
        changed();
        StructureChanged?.Invoke(this, EventArgs.Empty);
    }

    public void DeleteSelected()
    {
        string output = asset["graph"]!["output"]!.GetValue<string>();
        HashSet<string> removed = SelectedNodes.OfType<MaterialGraphNodeViewModel>().Select(node => node.Id).Where(id => id != output).ToHashSet();
        if (removed.Count == 0)
            return;
        foreach (JsonObject node in GraphNodes.OfType<JsonObject>().ToArray())
        {
            string id = node["id"]!.GetValue<string>();
            if (removed.Contains(id))
            {
                GraphNodes.Remove(node);
                Nodes.Remove(byId[id]);
                byId.Remove(id);
                positions().Remove(id);
            }
            else
                foreach ((string key, JsonNode? value) in inputs(node).ToArray())
                    if (value is JsonObject link && removed.Contains(link["node"]?.GetValue<string>() ?? string.Empty))
                        inputs(node).Remove(key);
        }
        SelectedNodes.Clear();
        refreshConnections();
        changed();
        StructureChanged?.Invoke(this, EventArgs.Empty);
    }

    public void Copy()
    {
        clipboard = new JsonArray(SelectedNodes.OfType<MaterialGraphNodeViewModel>().Where(node => node.Type != "Surface")
            .Select(node => node.Data.DeepClone()).ToArray());
    }

    public void Paste(Point position)
    {
        if (clipboard is null || clipboard.Count == 0)
            return;
        Dictionary<string, string> ids = clipboard.OfType<JsonObject>().ToDictionary(node => node["id"]!.GetValue<string>(), _ => Guid.NewGuid().ToString("N"));
        SelectedNodes.Clear();
        int offset = 0;
        foreach (JsonObject original in clipboard.OfType<JsonObject>())
        {
            JsonObject node = (JsonObject)original.DeepClone();
            string id = ids[node["id"]!.GetValue<string>()];
            node["id"] = id;
            foreach ((string key, JsonNode? value) in inputs(node).ToArray())
                if (value is JsonObject link)
                {
                    if (ids.TryGetValue(link["node"]?.GetValue<string>() ?? string.Empty, out string? replacement))
                        link["node"] = replacement;
                    else
                        inputs(node).Remove(key);
                }
            GraphNodes.Add(node);
            positions()[id] = new JsonArray(position.X + offset, position.Y + offset);
            SelectedNodes.Add(addView(node));
            offset += 48;
        }
        refreshConnections();
        changed();
        StructureChanged?.Invoke(this, EventArgs.Empty);
    }

    public void ShowDiagnostics(JsonArray? diagnostics)
    {
        foreach (MaterialGraphNodeViewModel node in byId.Values)
            node.SetDiagnostic(null);
        foreach (JsonObject diagnostic in (diagnostics ?? []).OfType<JsonObject>())
            if (byId.TryGetValue(diagnostic["nodeId"]?.GetValue<string>() ?? string.Empty, out MaterialGraphNodeViewModel? node))
                node.SetDiagnostic(diagnostic["message"]?.GetValue<string>());
    }

    public MaterialGraphNodeViewModel? FindNode(string id) => byId.GetValueOrDefault(id);

    public (double Zoom, double X, double Y) View => (JsonScalar.Number(asset["editor"]?["view"]?["zoom"], 1),
        JsonScalar.Number(asset["editor"]?["view"]?["offset"]?[0]), JsonScalar.Number(asset["editor"]?["view"]?["offset"]?[1]));

    public void SetView(double zoom, double x, double y)
    {
        if (View == (zoom, x, y))
            return;
        if (asset["editor"] is not JsonObject editor)
            asset["editor"] = editor = new JsonObject();
        editor["view"] = new JsonObject { ["zoom"] = zoom, ["offset"] = new JsonArray(x, y) };
        changed();
    }

    public void RefreshParameterNames()
    {
        JsonArray next = new((asset["parameters"] as JsonArray ?? []).OfType<JsonObject>().Select(parameter => (JsonNode)new JsonObject
        {
            ["label"] = parameter["displayName"]?.DeepClone() ?? parameter["id"]!.DeepClone(),
            ["value"] = parameter["id"]!.DeepClone(),
        }).ToArray());
        bool choicesChanged = !JsonNode.DeepEquals(parameterChoices, next);
        if (choicesChanged)
            parameterChoices = next;
        foreach (MaterialGraphNodeViewModel node in byId.Values.Where(node => node.Type == "Parameter"))
        {
            string id = node.Data["properties"]?["id"]?.GetValue<string>() ?? string.Empty;
            JsonObject? parameter = (asset["parameters"] as JsonArray)?.OfType<JsonObject>().FirstOrDefault(item => item["id"]?.GetValue<string>() == id);
            node.Title = "Parameter: " + (parameter?["displayName"]?.GetValue<string>() ?? id);
            if (choicesChanged)
                node.RefreshInline();
        }
    }

    private bool reaches(string current, string target, HashSet<string> visited)
    {
        if (current == target)
            return true;
        return visited.Add(current) && inputs(byId[current].Data).Any(pair => pair.Value is JsonObject link
            && link["node"] is JsonValue value && byId.ContainsKey(value.GetValue<string>()) && reaches(value.GetValue<string>(), target, visited));
    }

    private MaterialGraphNodeViewModel addView(JsonObject node)
    {
        string id = node["id"]!.GetValue<string>();
        JsonArray? position = asset["editor"]?["positions"]?[id] as JsonArray;
        JsonObject descriptor = descriptors.GetValueOrDefault(node["type"]!.GetValue<string>()) ?? new JsonObject();
        MaterialGraphNodeViewModel view = new(this, node, descriptor, new Point(position?[0]?.GetValue<double>() ?? 0, position?[1]?.GetValue<double>() ?? 0), moved =>
        {
            positions()[moved.Id] = new JsonArray(moved.Location.X, moved.Location.Y);
            changed();
        });
        byId[id] = view;
        Nodes.Add(view);
        RefreshParameterNames();
        return view;
    }

    private void refreshConnections()
    {
        Connections.Clear();
        foreach (MaterialGraphNodeViewModel node in byId.Values)
            foreach (MaterialGraphPortViewModel port in node.Input.Concat(node.Output).OfType<MaterialGraphPortViewModel>())
                port.IsConnected = false;
        foreach (MaterialGraphNodeViewModel node in byId.Values)
            foreach ((string name, JsonNode? value) in inputs(node.Data))
                if (value is JsonObject link && byId.TryGetValue(link["node"]?.GetValue<string>() ?? string.Empty, out MaterialGraphNodeViewModel? source))
                {
                    MaterialGraphPortViewModel? from = source.Output.OfType<MaterialGraphPortViewModel>().FirstOrDefault(port => port.Id == link["port"]?.GetValue<string>());
                    MaterialGraphPortViewModel? to = node.Input.OfType<MaterialGraphPortViewModel>().FirstOrDefault(port => port.Id == name);
                    if (from is null || to is null)
                        continue;
                    from.IsConnected = to.IsConnected = true;
                    Connections.Add(new MaterialGraphConnectionViewModel(this, from, to));
                }
        foreach (MaterialGraphNodeViewModel node in byId.Values)
            foreach (MaterialGraphPortViewModel port in node.Input.OfType<MaterialGraphPortViewModel>())
                port.RefreshInline();
    }

    private JsonObject positions()
    {
        if (asset["editor"] is not JsonObject editor)
            asset["editor"] = editor = new JsonObject();
        if (editor["positions"] is not JsonObject result)
            editor["positions"] = result = new JsonObject();
        return result;
    }

    private static JsonObject inputs(JsonObject node) => node["inputs"] as JsonObject ?? new JsonObject();

    private static JsonObject writableInputs(JsonObject node)
    {
        if (node["inputs"] is not JsonObject result)
            node["inputs"] = result = new JsonObject();
        return result;
    }
}
