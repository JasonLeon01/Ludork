using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private LuaEnumDefinition readEnum(string moduleName)
    {
        return new LuaEnumService(ProjectDirectory).Read(moduleName);
    }

    private Control createEnumEditor(
        LuaMetadataType type,
        JsonNode? value,
        Action<JsonNode?, bool> changed)
    {
        ComboBox selector = new()
        {
            HorizontalAlignment = HorizontalAlignment.Stretch,
            MinWidth = 180,
        };
        TextBlock diagnostic = new()
        {
            Foreground = Brushes.OrangeRed,
            TextWrapping = TextWrapping.Wrap,
        };
        StackPanel panel = new() { Spacing = 3, Children = { selector, diagnostic } };
        bool refreshing = false;
        void refresh(LuaEnumDefinition definition)
        {
            refreshing = true;
            selector.ItemsSource = definition.Options;
            BlueprintVariableOption? selected = definition.Options.FirstOrDefault(option => JsonNode.DeepEquals(option.Value, value));
            selector.SelectedItem = selected;
            selector.PlaceholderText = value is null ? LocaleService.Get("ENUM_SELECT_VALUE") : getText(value);
            diagnostic.Text = definition.Error
                ?? (selected is null && value is not null ? LocaleService.Get("ENUM_UNKNOWN_VALUE").Replace("{value}", getText(value), StringComparison.Ordinal) : string.Empty);
            diagnostic.IsVisible = !string.IsNullOrEmpty(diagnostic.Text);
            refreshing = false;
        }
        Window? window = null;
        List<Visual> ancestors = [];
        void visibilityChanged(object? sender, AvaloniaPropertyChangedEventArgs args)
        {
            if (args.Property == IsVisibleProperty && selector.IsEffectivelyVisible)
                refresh(readEnum(type.Name));
        }
        void refreshVisible(object? sender, EventArgs args)
        {
            if (selector.IsEffectivelyVisible)
                refresh(readEnum(type.Name));
        }
        selector.DropDownOpened += (_, _) => refresh(readEnum(type.Name));
        selector.AttachedToVisualTree += (_, _) =>
        {
            ancestors = selector.GetVisualAncestors().ToList();
            foreach (Visual ancestor in ancestors)
                ancestor.PropertyChanged += visibilityChanged;
            window = TopLevel.GetTopLevel(selector) as Window;
            if (window is not null)
                window.Activated += refreshVisible;
            refresh(readEnum(type.Name));
        };
        selector.DetachedFromVisualTree += (_, _) =>
        {
            if (window is not null)
                window.Activated -= refreshVisible;
            window = null;
            foreach (Visual ancestor in ancestors)
                ancestor.PropertyChanged -= visibilityChanged;
            ancestors.Clear();
        };
        selector.PropertyChanged += visibilityChanged;
        selector.SelectionChanged += (_, _) =>
        {
            if (refreshing || selector.SelectedItem is not BlueprintVariableOption selection)
                return;
            LuaEnumDefinition current = readEnum(type.Name);
            BlueprintVariableOption? chosen = current.Options.FirstOrDefault(option => option.Label == selection.Label);
            if (chosen is not null && !JsonNode.DeepEquals(value, chosen.Value))
            {
                value = chosen.Value?.DeepClone();
                changed(value?.DeepClone(), false);
            }
            refresh(current);
        };
        refresh(readEnum(type.Name));
        return panel;
    }
}
