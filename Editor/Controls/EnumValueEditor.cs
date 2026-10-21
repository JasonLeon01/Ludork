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

public sealed class EnumValueEditor : StackPanel
{
    public EnumValueEditor(
        Func<LuaEnumDefinition> readEnum,
        JsonNode? value,
        Action<JsonNode?> changed,
        bool allowEmptyString = false)
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
        Spacing = 3;
        Children.Add(selector);
        Children.Add(diagnostic);
        bool refreshing = false;
        BlueprintVariableOption empty = new(LocaleService.Get("GENERAL_DATA_PLACEHOLDER"), JsonValue.Create(string.Empty));
        IReadOnlyList<BlueprintVariableOption> options(LuaEnumDefinition definition)
        {
            return allowEmptyString && definition.ValueType?.Name == "string"
                ? new[] { empty }.Concat(definition.Options.Where(option => !JsonNode.DeepEquals(option.Value, empty.Value))).ToArray()
                : definition.Options;
        }
        void refresh(LuaEnumDefinition definition)
        {
            refreshing = true;
            selector.ItemsSource = options(definition);
            BlueprintVariableOption? selected = options(definition).FirstOrDefault(option => JsonNode.DeepEquals(option.Value, value));
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
                refresh(readEnum());
        }
        void refreshVisible(object? sender, EventArgs args)
        {
            if (selector.IsEffectivelyVisible)
                refresh(readEnum());
        }
        selector.DropDownOpened += (_, _) => refresh(readEnum());
        selector.AttachedToVisualTree += (_, _) =>
        {
            ancestors = selector.GetVisualAncestors().ToList();
            foreach (Visual ancestor in ancestors)
                ancestor.PropertyChanged += visibilityChanged;
            window = TopLevel.GetTopLevel(selector) as Window;
            if (window is not null)
                window.Activated += refreshVisible;
            refresh(readEnum());
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
            LuaEnumDefinition current = readEnum();
            BlueprintVariableOption? chosen = options(current).FirstOrDefault(option => option.Label == selection.Label);
            if (chosen is not null && !JsonNode.DeepEquals(value, chosen.Value))
            {
                value = chosen.Value?.DeepClone();
                changed(value?.DeepClone());
            }
            refresh(current);
        };
        refresh(readEnum());
    }

    private static string getText(JsonNode? value)
    {
        if (value is null)
            return string.Empty;
        return JsonScalar.TryGetString(value, out string text) ? text : value.ToJsonString();
    }
}
