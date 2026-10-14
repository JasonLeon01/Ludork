using Avalonia;
using Avalonia.Media;
using Ludork.Services;
using NodifyM.Avalonia.ViewModelBase;
using System;
using System.ComponentModel;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels.Materials;

public sealed class MaterialGraphNodeViewModel : NodeViewModelBase
{
    private string? diagnostic;

    public MaterialGraphNodeViewModel(MaterialGraphViewModel graph, JsonObject data, JsonObject descriptor, Point position, Action<MaterialGraphNodeViewModel> moved)
    {
        Graph = graph;
        Data = data;
        Descriptor = descriptor;
        Title = descriptor["displayName"]?.GetValue<string>() ?? Type;
        Location = position;
        addPorts(descriptor["inputs"] as JsonArray, true);
        addPorts(descriptor["outputs"] as JsonArray, false);
        PropertyChanged += (_, args) =>
        {
            if (args.PropertyName == nameof(Location))
                moved(this);
        };
    }

    public JsonObject Data { get; }
    public MaterialGraphViewModel Graph { get; }
    public JsonObject Descriptor { get; }
    public string Id => Data["id"]!.GetValue<string>();
    public string Type => Data["type"]!.GetValue<string>();
    public string? Diagnostic => diagnostic;
    public IBrush HeaderBrush => EditorTheme.Brush(diagnostic is null ? "AccentMuted" : "Error");
    public event EventHandler? InlineStateChanged;

    public void RefreshInline() => InlineStateChanged?.Invoke(this, EventArgs.Empty);

    public void SetDiagnostic(string? value)
    {
        diagnostic = value;
        OnPropertyChanged(nameof(Diagnostic));
        OnPropertyChanged(nameof(HeaderBrush));
    }

    private void addPorts(JsonArray? descriptors, bool input)
    {
        if (descriptors is null)
            return;
        foreach (JsonObject descriptor in descriptors.OfType<JsonObject>())
        {
            MaterialGraphPortViewModel port = new(this, descriptor, input);
            port.Initialize();
            if (input)
                Input.Add(port);
            else
                Output.Add(port);
        }
    }
}
