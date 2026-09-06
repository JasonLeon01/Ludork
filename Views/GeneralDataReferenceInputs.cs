using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views;

internal static class GeneralDataReferenceInputs
{
    public static ComboBox Create(string current, IReadOnlyList<string> options)
    {
        ComboBox combo = new()
        {
            MinHeight = EditorInputs.FieldMinHeight,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        ComboBoxItem placeholder = createItem(LocaleService.Get("GENERAL_DATA_PLACEHOLDER"), string.Empty);
        combo.Items.Add(placeholder);
        ComboBoxItem selected = placeholder;
        if (current.Length > 0 && !options.Contains(current))
        {
            selected = createItem(current, current);
            combo.Items.Add(selected);
        }
        foreach (string option in options)
        {
            if (option.Length == 0)
                continue;
            ComboBoxItem item = createItem(option, option);
            combo.Items.Add(item);
            if (string.Equals(option, current, StringComparison.Ordinal))
                selected = item;
        }
        combo.SelectedItem = selected;
        return combo;
    }

    public static string GetValue(ComboBox combo)
    {
        return (combo.SelectedItem as ComboBoxItem)?.Tag as string ?? string.Empty;
    }

    private static ComboBoxItem createItem(string label, string value)
    {
        return new ComboBoxItem
        {
            Content = label,
            Tag = value,
        };
    }
}
