using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Views;

public sealed partial class SubtitleWindow
{
    private IEnumerable<JsonObject> multilingualContent => sections.OfType<JsonObject>()
        .Select(item => item["content"]).OfType<JsonObject>();

    private string[] languageKeys() => multilingualContent.SelectMany(content => content.Select(pair => pair.Key))
        .Distinct(StringComparer.Ordinal).ToArray();

    private bool normalizeContent()
    {
        bool changed = false;
        string[] keys = languageKeys();
        foreach (JsonObject content in multilingualContent)
        {
            int count = content.Select(pair => pair.Value).OfType<JsonArray>().Select(lines => lines.Count).DefaultIfEmpty().Max();
            foreach (string key in keys)
            {
                if (!content.ContainsKey(key))
                {
                    content[key] = new JsonArray();
                    changed = true;
                }
                if (content[key] is not JsonArray lines)
                    continue;
                while (lines.Count < count)
                {
                    lines.Add(string.Empty);
                    changed = true;
                }
            }
        }
        return changed;
    }

    private void editContent(Action action)
    {
        normalizeContent();
        action();
        commit();
        buildProperties();
    }

    private Button symbolButton(string symbol, Action action)
    {
        Button result = new()
        {
            Content = symbol,
            Width = 28,
            Height = 28,
            Padding = new Thickness(0),
            HorizontalContentAlignment = HorizontalAlignment.Center,
            VerticalContentAlignment = VerticalAlignment.Center,
        };
        result.Click += (_, _) =>
        {
            if (inputErrors.Count == 0)
                action();
        };
        return result;
    }

    private void buildLanguages(JsonObject languages)
    {
        string[] keys = languages.Select(pair => pair.Key).ToArray();
        if (!languages.ContainsKey(selectedLanguage))
            selectedLanguage = keys.FirstOrDefault() ?? "en_GB";
        Grid columns = new() { ColumnDefinitions = new ColumnDefinitions("130,8,*") };
        StackPanel left = new() { Spacing = 5 };
        StackPanel right = new() { Spacing = 6 };
        ListBox list = new() { MinHeight = 80 };
        List<ListBoxItem> items = [];
        foreach (string key in keys)
        {
            ListBoxItem item = new() { Content = key, Tag = key };
            MenuItem rename = new() { Header = L("SUBTITLE_RENAME_LANGUAGE") };
            rename.Click += async (_, _) =>
            {
                if (inputErrors.Count != 0)
                    return;
                string? next = await SingleRowDialog.ShowAsync(this, L("SUBTITLE_RENAME_LANGUAGE"), L("NAME"),
                    languageKeys().Where(name => name != key), key);
                if (next is null || next == key || inputErrors.Count != 0)
                    return;
                editContent(() =>
                {
                    foreach (JsonObject content in multilingualContent)
                    {
                        int index = content.IndexOf(key);
                        JsonNode? lines = content[key];
                        content.Remove(key);
                        content.Insert(index, next, lines);
                    }
                    selectedLanguage = next;
                });
            };
            item.ContextMenu = new ContextMenu { ItemsSource = new[] { rename } };
            items.Add(item);
        }
        list.ItemsSource = items;
        list.SelectedIndex = Array.IndexOf(keys, selectedLanguage);
        list.SelectionChanged += (_, _) =>
        {
            if (refreshing)
                return;
            if (inputErrors.Count != 0)
            {
                refreshing = true;
                list.SelectedIndex = Array.IndexOf(keys, selectedLanguage);
                refreshing = false;
                return;
            }
            if (list.SelectedItem is not ListBoxItem { Tag: string key })
                return;
            selectedLanguage = key;
            refreshing = true;
            right.Children.Clear();
            buildLines(right, languages[selectedLanguage] as JsonArray);
            refreshing = false;
        };
        left.Children.Add(list);
        StackPanel actions = new() { Orientation = Orientation.Horizontal, Spacing = 5 };
        actions.Children.Add(symbolButton("+", () => editContent(() =>
        {
            string[] existing = languageKeys();
            string name = "en_GB";
            int suffix = 1;
            while (existing.Contains(name, StringComparer.Ordinal))
                name = "language_" + suffix++;
            foreach (JsonObject content in multilingualContent)
                content[name] = new JsonArray();
            normalizeContent();
            selectedLanguage = name;
        })));
        Button remove = symbolButton("−", () => editContent(() =>
        {
            foreach (JsonObject content in multilingualContent)
                content.Remove(selectedLanguage);
        }));
        remove.IsEnabled = keys.Length != 0;
        actions.Children.Add(remove);
        left.Children.Add(actions);
        columns.Children.Add(left);
        buildLines(right, languages[selectedLanguage] as JsonArray);
        Grid.SetColumn(right, 2);
        columns.Children.Add(right);
        properties.Children.Add(columns);
    }

    private IEnumerable<JsonArray> lineArrays(JsonArray lines) => section?["content"] is JsonObject languages
        ? languages.Select(pair => pair.Value).OfType<JsonArray>() : new[] { lines };

    private void buildLines(StackPanel panel, JsonArray? lines)
    {
        int count = lines?.Count ?? 0;
        selectedLine = count == 0 ? -1 : Math.Clamp(selectedLine, 0, count - 1);
        List<Border> rows = [];
        Button up = symbolButton("↑", () => moveLine(lines!, -1));
        Button down = symbolButton("↓", () => moveLine(lines!, 1));
        Button add = symbolButton("+", () =>
        {
            editContent(() =>
            {
                foreach (JsonArray array in lineArrays(lines!))
                    array.Add(string.Empty);
                selectedLine = lines!.Count - 1;
            });
            focusLine();
        });
        Button remove = symbolButton("−", () =>
        {
            editContent(() =>
            {
                foreach (JsonArray array in lineArrays(lines!))
                    array.RemoveAt(selectedLine);
                selectedLine = Math.Min(selectedLine, lines!.Count - 1);
            });
            focusLine();
        });
        void refreshSelection()
        {
            up.IsEnabled = selectedLine > 0;
            down.IsEnabled = selectedLine >= 0 && selectedLine + 1 < count;
            add.IsEnabled = lines is not null;
            remove.IsEnabled = selectedLine >= 0;
            for (int index = 0; index < rows.Count; index++)
                rows[index].BorderBrush = index == selectedLine ? EditorTheme.Brush("Accent") : Brushes.Transparent;
        }
        for (int index = 0; index < count; index++)
        {
            int lineIndex = index;
            string text = lines![index] is JsonValue scalar && scalar.TryGetValue(out string? value)
                ? value ?? string.Empty : lines[index]?.ToJsonString() ?? string.Empty;
            TextBox input = EditorInputs.CreateEditableTextBox(text);
            input.Tag = lineIndex;
            input.AcceptsReturn = true;
            input.TextWrapping = TextWrapping.Wrap;
            input.MinHeight = 65;
            input.GotFocus += (_, _) => { selectedLine = lineIndex; refreshSelection(); };
            input.AddHandler(PointerPressedEvent, (_, _) => { selectedLine = lineIndex; refreshSelection(); }, RoutingStrategies.Tunnel);
            input.TextChanged += (_, _) =>
            {
                if (refreshing || input.Text == text)
                    return;
                text = input.Text ?? string.Empty;
                bool normalized = normalizeContent();
                lines[lineIndex] = text;
                commit();
                if (normalized)
                {
                    int caret = input.CaretIndex;
                    buildProperties();
                    focusLine(caret);
                }
            };
            Border row = new() { Child = input, BorderThickness = new Thickness(1), CornerRadius = new CornerRadius(4) };
            rows.Add(row);
            panel.Children.Add(row);
        }
        StackPanel actions = new() { Orientation = Orientation.Horizontal, Spacing = 5 };
        actions.Children.Add(up);
        actions.Children.Add(down);
        actions.Children.Add(add);
        actions.Children.Add(remove);
        panel.Children.Add(actions);
        refreshSelection();
    }

    private void focusLine(int? caret = null)
    {
        TextBox? input = Avalonia.LogicalTree.LogicalExtensions.GetLogicalDescendants(properties)
            .OfType<TextBox>().FirstOrDefault(box => box.Tag is int index && index == selectedLine);
        if (input is null)
            return;
        input.Focus();
        if (caret is int position)
            input.CaretIndex = position;
    }

    private void moveLine(JsonArray lines, int delta)
    {
        editContent(() =>
        {
            foreach (JsonArray array in lineArrays(lines))
            {
                JsonNode? item = array[selectedLine];
                array.RemoveAt(selectedLine);
                array.Insert(selectedLine + delta, item);
            }
            selectedLine += delta;
        });
        focusLine();
    }
}
