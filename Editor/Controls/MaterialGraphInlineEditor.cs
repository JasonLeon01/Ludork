using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels.Materials;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class MaterialGraphInlineEditor : ContentControl
{
    private readonly List<BlueprintVariableForm> forms = [];
    private object? model;
    private bool attached;

    public MaterialGraphInlineEditor()
    {
        HorizontalContentAlignment = HorizontalAlignment.Stretch;
        DataContextChanged += (_, _) => bind();
        SizeChanged += (_, _) => this.FindAncestorOfType<MaterialGraphControl>()?.RequestAnchorRefresh();
        AddHandler(PointerPressedEvent, stopInput, RoutingStrategies.Bubble, true);
        AddHandler(PointerMovedEvent, stopInput, RoutingStrategies.Bubble, true);
        AddHandler(PointerReleasedEvent, stopInput, RoutingStrategies.Bubble, true);
        AddHandler(PointerWheelChangedEvent, stopInput, RoutingStrategies.Bubble, true);
        AddHandler(KeyDownEvent, stopInput, RoutingStrategies.Bubble, true);
        AddHandler(KeyUpEvent, stopInput, RoutingStrategies.Bubble, true);
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        attached = true;
        bind();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        attached = false;
        release();
        base.OnDetachedFromVisualTree(args);
    }

    private static void stopInput(object? sender, RoutedEventArgs args) => args.Handled = true;

    private void bind()
    {
        if (!attached || ReferenceEquals(model, DataContext))
            return;
        release();
        model = DataContext;
        if (model is MaterialGraphPortViewModel port)
            port.InlineStateChanged += onStateChanged;
        else if (model is MaterialGraphNodeViewModel node)
            node.InlineStateChanged += onStateChanged;
        rebuild();
    }

    private void onStateChanged(object? sender, EventArgs args) => rebuild();

    private void rebuild()
    {
        clearForms();
        Content = null;
        IsVisible = false;
        MaterialGraphControl? graph = this.FindAncestorOfType<MaterialGraphControl>();
        if (graph?.GameData is not ProjectDataStore gameData)
            return;
        StackPanel panel = new() { Spacing = 6, MinWidth = 180, MaxWidth = 260 };
        if (model is MaterialGraphPortViewModel port)
        {
            if (port.HasLink)
            {
                graph.RequestAnchorRefresh();
                return;
            }
            if (!port.HasInlineValue)
                panel.Children.Add(new TextBlock
                {
                    Text = LocaleService.Get(port.Descriptor["inherited"]?.GetValue<bool>() == true ? "MATERIAL_INHERITED_INPUT" : "MATERIAL_DEFAULT_INPUT"),
                    Opacity = 0.65,
                });
            else
                addField(panel, gameData, port.Node.Graph, port.Descriptor, port.Value, value => port.Node.Graph.SetInput(port, value), port.RefreshInline, false);
        }
        else if (model is MaterialGraphNodeViewModel node)
        {
            foreach (JsonObject original in (node.Descriptor["properties"] as JsonArray ?? []).OfType<JsonObject>())
            {
                JsonObject descriptor = node.Graph.PropertyDescriptor(node, original);
                string id = descriptor["id"]!.GetValue<string>();
                addField(panel, gameData, node.Graph, descriptor, node.Data["properties"]?[id], value => node.Graph.SetProperty(node, id, value), node.RefreshInline, true);
            }
        }
        if (panel.Children.Count > 0)
        {
            Content = panel;
            IsVisible = true;
        }
        graph.RequestAnchorRefresh();
    }

    private void addField(StackPanel panel, ProjectDataStore gameData, MaterialGraphViewModel graph, JsonObject descriptor,
        JsonNode? value, Action<JsonNode?> changed, Action refresh, bool showName)
    {
        if (descriptor["type"]?.GetValue<string>() == "numeric")
        {
            string[] types = ["float", "vec2", "vec3", "vec4"];
            ComboBox shape = new()
            {
                ItemsSource = types,
                SelectedIndex = (value ?? descriptor["default"]) is JsonArray vector ? vector.Count - 1 : 0,
                HorizontalAlignment = HorizontalAlignment.Stretch,
            };
            shape.SelectionChanged += (_, _) =>
            {
                if (shape.SelectedItem is not string type)
                    return;
                changed(graph.DefaultValue(type));
                refresh();
            };
            panel.Children.Add(shape);
        }
        BlueprintVariableForm form = MaterialPropertyForm.Create(gameData, descriptor, value, changed, showName);
        forms.Add(form);
        panel.Children.Add(form);
    }

    private void clearForms()
    {
        foreach (BlueprintVariableForm form in forms)
            form.Dispose();
        forms.Clear();
    }

    private void release()
    {
        if (model is MaterialGraphPortViewModel port)
            port.InlineStateChanged -= onStateChanged;
        else if (model is MaterialGraphNodeViewModel node)
            node.InlineStateChanged -= onStateChanged;
        clearForms();
        model = null;
        Content = null;
    }
}
