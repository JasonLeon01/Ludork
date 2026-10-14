using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class MaterialOptionEditor : StackPanel
{
    public MaterialOptionEditor(IReadOnlyList<BlueprintVariableOption> options, JsonNode? initial, Action<JsonNode?, bool> changed)
    {
        Spacing = 4;
        BlueprintVariableOption? selected = options.FirstOrDefault(option => JsonNode.DeepEquals(option.Value, initial));
        ComboBox selector = new()
        {
            ItemsSource = options,
            SelectedItem = selected,
            PlaceholderText = initial?.ToString() ?? string.Empty,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        TextBlock error = new()
        {
            Text = $"{LocaleService.Get("MATERIAL_INVALID_OPTION")}: {initial}",
            Foreground = Brushes.OrangeRed,
            TextWrapping = TextWrapping.Wrap,
            IsVisible = selected is null,
        };
        Children.Add(selector);
        Children.Add(error);
        selector.SelectionChanged += (_, _) =>
        {
            if (selector.SelectedItem is not BlueprintVariableOption next || JsonNode.DeepEquals(next.Value, initial))
                return;
            initial = next.Value?.DeepClone();
            error.IsVisible = false;
            changed(initial, false);
        };
    }
}
