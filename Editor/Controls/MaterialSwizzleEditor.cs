using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

internal sealed class MaterialSwizzleEditor : StackPanel
{
    private readonly JsonObject[] channels;
    private readonly int[] lengths;
    private readonly ComboBox length;
    private readonly StackPanel components = new() { Spacing = 4 };
    private readonly TextBlock error = new() { Foreground = Brushes.OrangeRed, TextWrapping = TextWrapping.Wrap };
    private readonly Action<JsonNode?, bool> changed;
    private string value;

    public MaterialSwizzleEditor(JsonObject descriptor, JsonNode? initial, Action<JsonNode?, bool> commit)
    {
        Spacing = 6;
        changed = commit;
        value = JsonScalar.TryGetString(initial, out string text) ? text : initial?.ToJsonString() ?? string.Empty;
        channels = descriptor["channels"]!.AsArray().OfType<JsonObject>().ToArray();
        lengths = descriptor["lengths"]!.AsArray().Select(item => item!.GetValue<int>()).ToArray();
        length = new ComboBox
        {
            ItemsSource = lengths,
            SelectedIndex = Array.IndexOf(lengths, value.Length),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        Children.Add(new TextBlock { Text = LocaleService.Get("MATERIAL_SWIZZLE_LENGTH") });
        Children.Add(length);
        Children.Add(components);
        Children.Add(error);
        rebuild();
        length.SelectionChanged += (_, _) =>
        {
            if (length.SelectedItem is not int size || value.Length == size)
                return;
            value = value.Length > size ? value[..size] : value.PadRight(size, channels[0]["value"]!.GetValue<string>()[0]);
            rebuild();
            changed(JsonValue.Create(value), false);
        };
    }

    private void rebuild()
    {
        components.Children.Clear();
        bool valid = lengths.Contains(value.Length);
        for (int index = 0; index < Math.Min(value.Length, lengths.Max()); index++)
        {
            int component = index;
            string current = value[index].ToString();
            int selected = Array.FindIndex(channels, channel => channel["value"]!.GetValue<string>() == current
                || channel["aliases"]!.AsArray().Any(alias => alias!.GetValue<string>() == current));
            valid &= selected >= 0;
            ComboBox selector = new()
            {
                ItemsSource = channels.Select(channel => channel["label"]!.GetValue<string>()).ToArray(),
                SelectedIndex = selected,
                PlaceholderText = current,
                HorizontalAlignment = HorizontalAlignment.Stretch,
            };
            components.Children.Add(selector);
            selector.SelectionChanged += (_, _) =>
            {
                if (selector.SelectedIndex < 0)
                    return;
                char[] next = value.ToCharArray();
                next[component] = channels[selector.SelectedIndex]["value"]!.GetValue<string>()[0];
                string updated = new(next);
                if (updated == value)
                    return;
                value = updated;
                refreshError();
                changed(JsonValue.Create(value), false);
            };
        }
        showError(valid);
    }

    private void refreshError()
    {
        showError(lengths.Contains(value.Length) && value.All(character => channels.Any(channel =>
            channel["value"]!.GetValue<string>() == character.ToString()
            || channel["aliases"]!.AsArray().Any(alias => alias!.GetValue<string>() == character.ToString()))));
    }

    private void showError(bool valid)
    {
        error.IsVisible = !valid;
        error.Text = valid ? string.Empty : $"{LocaleService.Get("MATERIAL_SWIZZLE_INVALID")}: {value}";
    }
}
