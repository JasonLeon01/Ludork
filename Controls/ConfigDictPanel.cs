using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class ConfigDictPanel : Border
{
    private readonly Window owner;
    private readonly GameDataService gameData;
    private readonly string fileName;
    private readonly JsonObject data;
    private readonly StackPanel content = new() { Spacing = 8 };

    public ConfigDictPanel(Window owner, GameDataService gameData, string fileName, JsonObject data)
    {
        this.owner = owner;
        this.gameData = gameData;
        this.fileName = fileName;
        this.data = data;
        Background = new SolidColorBrush(Color.FromRgb(43, 43, 43));
        BorderBrush = new SolidColorBrush(Color.FromRgb(96, 96, 96));
        BorderThickness = new Thickness(1);
        CornerRadius = new CornerRadius(6);
        Padding = new Thickness(8);
        Child = content;
        HistoryMergeBehavior.AttachBoundary(owner, gameData);
        rebuild();
    }

    private void rebuild()
    {
        content.Children.Clear();
        content.Children.Add(new TextBlock
        {
            Text = fileName,
            FontSize = 16,
            FontWeight = FontWeight.Bold,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
        });

        Grid form = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,8,*"),
            RowDefinitions = new RowDefinitions(),
            RowSpacing = 8,
        };
        int row = 0;
        foreach (KeyValuePair<string, JsonNode?> entry in data)
        {
            if (entry.Value is not JsonObject value)
                continue;
            (string type, int? length) = parseType(value["type"]?.GetValue<string>() ?? string.Empty);
            string baseType = type;
            if (length is null && baseType.EndsWith("[]", StringComparison.Ordinal))
                baseType = baseType[..^2];
            bool isArray = length is not null || type.EndsWith("[]", StringComparison.Ordinal);
            form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
            TextBlock label = new()
            {
                Text = LocaleService.Get(entry.Key),
                VerticalAlignment = isArray ? VerticalAlignment.Top : VerticalAlignment.Center,
                Margin = isArray ? new Thickness(0, 4, 0, 0) : default,
            };
            Grid.SetColumn(label, 0);
            Grid.SetRow(label, row);
            form.Children.Add(label);
            Control editor = isArray ? createArrayEditor(baseType, length, value) : createSingleEditor(baseType, value);
            Grid.SetColumn(editor, 2);
            Grid.SetRow(editor, row);
            form.Children.Add(editor);
            row += 1;
        }
        content.Children.Add(form);
    }

    private Control createSingleEditor(string type, JsonObject value)
    {
        if (type == "file")
            return createFileEditor(value, value["value"]?.GetValue<string>() ?? string.Empty, null, null, false, null);
        if (isNumericType(type))
        {
            NumericUpDown edit = createNumericUpDown(type, value["value"]);
            HistoryMergeBehavior.Attach(edit, gameData);
            edit.ValueChanged += (_, _) => updateScalar(type, edit.Value, value);
            return edit;
        }
        TextBox text = createTextBox(value["value"]);
        HistoryMergeBehavior.Attach(text, gameData);
        text.TextChanged += (_, _) => updateScalar(text.Text, value);
        return text;
    }

    private Control createArrayEditor(string type, int? length, JsonObject value)
    {
        JsonArray values = getValues(value);
        if (length is int fixedLength)
        {
            while (values.Count < fixedLength)
                values.Add(defaultValue(type));
            while (values.Count > fixedLength)
                values.RemoveAt(values.Count - 1);
            value["value"] = values;
            if (isNumericType(type))
                return createFixedNumericRow(type, value, values);
        }

        StackPanel rows = new() { Spacing = 6 };
        void refreshRows()
        {
            rows.Children.Clear();
            for (int index = 0; index < values.Count; index++)
                rows.Children.Add(createArrayRow(type, value, values, index, length is null, refreshRows));
            if (length is null)
            {
                Button add = new()
                {
                    Content = "+",
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                };
                add.Click += (_, _) =>
                {
                    gameData.RecordSnapshot();
                    values.Add(defaultValue(type));
                    value["value"] = values;
                    markModified();
                    refreshRows();
                };
                rows.Children.Add(add);
            }
        }
        refreshRows();
        return rows;
    }

    private Control createFixedNumericRow(string type, JsonObject value, JsonArray values)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions(string.Join(",", Enumerable.Repeat("*", values.Count))),
            ColumnSpacing = 6,
        };
        for (int index = 0; index < values.Count; index++)
        {
            int captured = index;
            NumericUpDown edit = createNumericUpDown(type, values[index]);
            HistoryMergeBehavior.Attach(edit, gameData);
            edit.ValueChanged += (_, _) =>
            {
                JsonNode? next = toJsonNumber(type, edit.Value);
                if (next is null || JsonNode.DeepEquals(values[captured], next))
                    return;
                gameData.RecordSnapshot();
                values[captured] = next;
                value["value"] = values;
                markModified();
            };
            Grid.SetColumn(edit, index);
            row.Children.Add(edit);
        }
        return row;
    }

    private Control createArrayRow(
        string type,
        JsonObject value,
        JsonArray values,
        int index,
        bool variableLength,
        Action refreshRows)
    {
        void removeAt()
        {
            gameData.RecordSnapshot();
            values.RemoveAt(index);
            value["value"] = values;
            markModified();
            refreshRows();
        }

        if (type == "file")
            return createFileEditor(
                value,
                values[index]?.GetValue<string>() ?? string.Empty,
                values,
                index,
                variableLength,
                variableLength ? removeAt : null);

        Grid row = new() { ColumnDefinitions = new ColumnDefinitions("*,Auto"), ColumnSpacing = 6 };
        Control editor = createArrayValueEditor(type, value, values, index);
        Grid.SetColumn(editor, 0);
        row.Children.Add(editor);
        if (variableLength)
        {
            Button remove = createMinusButton();
            remove.Click += (_, _) => removeAt();
            Grid.SetColumn(remove, 1);
            row.Children.Add(remove);
        }
        return row;
    }

    private Control createArrayValueEditor(string type, JsonObject value, JsonArray values, int index)
    {
        if (isNumericType(type))
        {
            NumericUpDown edit = createNumericUpDown(type, values[index]);
            HistoryMergeBehavior.Attach(edit, gameData);
            edit.ValueChanged += (_, _) =>
            {
                JsonNode? next = toJsonNumber(type, edit.Value);
                if (next is null || JsonNode.DeepEquals(values[index], next))
                    return;
                gameData.RecordSnapshot();
                values[index] = next;
                value["value"] = values;
                markModified();
            };
            return edit;
        }

        TextBox editText = createTextBox(values[index]);
        HistoryMergeBehavior.Attach(editText, gameData);
        editText.TextChanged += (_, _) =>
        {
            JsonNode next = JsonValue.Create(editText.Text ?? string.Empty)!;
            if (JsonNode.DeepEquals(values[index], next))
                return;
            gameData.RecordSnapshot();
            values[index] = next;
            value["value"] = values;
            markModified();
        };
        return editText;
    }

    private Control createFileEditor(
        JsonObject value,
        string initialValue,
        JsonArray? values,
        int? index,
        bool showRemove,
        Action? onRemove)
    {
        Grid row = new()
        {
            ColumnDefinitions = new ColumnDefinitions(showRemove ? "*,Auto,Auto" : "*,Auto"),
            ColumnSpacing = 6,
        };
        TextBox edit = EditorInputs.CreateReadOnlyTextBox(initialValue);
        Button browse = new() { Content = "..." };
        browse.Click += async (_, _) =>
        {
            string? selected = await selectFileName(value, edit.Text ?? string.Empty);
            if (selected is null)
                return;
            bool changed = values is not null && index is int selectedIndex
                ? !string.Equals(values[selectedIndex]?.GetValue<string>(), selected, StringComparison.Ordinal)
                : !string.Equals(value["value"]?.GetValue<string>(), selected, StringComparison.Ordinal);
            if (!changed)
                return;
            gameData.RecordSnapshot();
            edit.Text = selected;
            if (values is not null && index is int targetIndex)
                values[targetIndex] = selected;
            else
                value["value"] = selected;
            markModified();
        };
        Grid.SetColumn(edit, 0);
        row.Children.Add(edit);
        if (showRemove)
        {
            Button remove = createMinusButton();
            remove.Click += (_, _) => onRemove!();
            Grid.SetColumn(browse, 1);
            Grid.SetColumn(remove, 2);
            row.Children.Add(remove);
        }
        else
        {
            Grid.SetColumn(browse, 1);
        }
        row.Children.Add(browse);
        return row;
    }

    private static Button createMinusButton() => new()
    {
        Content = "-",
        Foreground = new SolidColorBrush(Color.FromRgb(231, 76, 60)),
    };

    private async Task<string?> selectFileName(JsonObject value, string current)
    {
        string root = getFileRoot(value);
        IReadOnlyList<string> extensions = getExtensions(value["ext"]);
        string filterStr = extensions.Count == 0
            ? FileSelectorDialog.AllFilesFilter(star: true)
            : FileSelectorDialog.FilesFilter(extensions.Select(e => "*" + e).ToArray());
        string? initialFilePath = string.IsNullOrWhiteSpace(current)
            ? null
            : Path.Combine(root, current);
        string? path = await FileSelectorDialog.ShowAsync(
            owner,
            root,
            filterStr,
            initialFilePath: initialFilePath);
        if (path is null) return null;
        string fileName = Path.GetFileName(path);
        if (extensions.Count != 0
            && !extensions.Any(e => fileName.EndsWith(e, StringComparison.OrdinalIgnoreCase)))
            return null;
        return Path.GetRelativePath(root, path).Replace('\\', '/');
    }

    private string getFileRoot(JsonObject value)
    {
        string root = value["root"]?.GetValue<string>() is { Length: > 0 } rootKey
            ? Path.Combine(gameData.ProjectPath, rootKey.Trim())
            : Path.Combine(gameData.ProjectPath, "Assets");
        string? basePath = value["base"]?.GetValue<string>();
        return string.IsNullOrWhiteSpace(basePath) ? root : Path.Combine(root, basePath.Trim());
    }

    private void updateScalar(string type, decimal? number, JsonObject value)
    {
        JsonNode? next = toJsonNumber(type, number);
        if (next is null || JsonNode.DeepEquals(value["value"], next))
            return;
        gameData.RecordSnapshot();
        value["value"] = next;
        markModified();
    }

    private void updateScalar(string? text, JsonObject value)
    {
        JsonNode next = JsonValue.Create(text ?? string.Empty)!;
        if (JsonNode.DeepEquals(value["value"], next))
            return;
        gameData.RecordSnapshot();
        value["value"] = next;
        markModified();
    }

    private static NumericUpDown createNumericUpDown(string type, JsonNode? initialValue)
    {
        bool isInt = type == "int";
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            isInt ? getInt(initialValue) : (decimal)getDouble(initialValue),
            isInt ? int.MinValue : (decimal)-999999999.0,
            isInt ? int.MaxValue : (decimal)999999999.0,
            isInt ? 1m : 0.1m);
        box.FormatString = isInt ? "0" : "0.##";
        box.ParsingNumberStyle = isInt ? NumberStyles.Integer : NumberStyles.Float;
        return box;
    }

    private static TextBox createTextBox(JsonNode? initialValue) =>
        EditorInputs.CreateEditableTextBox(initialValue?.ToString() ?? string.Empty);

    private static bool isNumericType(string type) => type is "int" or "float";

    private static JsonNode? toJsonNumber(string type, decimal? value)
    {
        if (value is null)
            return null;
        return type == "int"
            ? JsonValue.Create((int)Math.Round(value.Value, MidpointRounding.AwayFromZero))
            : JsonValue.Create((double)value.Value);
    }

    private static (string Type, int? Length) parseType(string type)
    {
        type = type.Trim();
        int open = type.IndexOf('[');
        int close = type.IndexOf(']');
        if (open < 0 || close <= open)
            return (type, null);
        string inner = type[(open + 1)..close];
        string baseType = type[..open];
        if (inner.Length == 0)
            return (baseType + "[]", null);
        return int.TryParse(inner, NumberStyles.Integer, CultureInfo.InvariantCulture, out int length)
            ? (baseType, Math.Max(0, length))
            : (type, null);
    }

    private static JsonArray getValues(JsonObject value)
    {
        if (value["value"] is JsonArray values)
            return values;
        JsonArray empty = new();
        value["value"] = empty;
        return empty;
    }

    private static JsonNode defaultValue(string type) => type switch
    {
        "int" => JsonValue.Create(0)!,
        "float" => JsonValue.Create(0.0)!,
        _ => JsonValue.Create(string.Empty)!,
    };

    private static int getInt(JsonNode? value) => int.TryParse(value?.ToString(), NumberStyles.Integer, CultureInfo.InvariantCulture, out int result) ? result : 0;

    private static double getDouble(JsonNode? value) => double.TryParse(value?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double result) ? result : 0;

    private static IReadOnlyList<string> getExtensions(JsonNode? value)
    {
        if (value is JsonValue && value.GetValue<string>() is { Length: > 0 } extension)
            return [extension];
        if (value is not JsonArray values)
            return [];
        return values.Select(item => item?.GetValue<string>()).Where(item => !string.IsNullOrWhiteSpace(item)).Cast<string>().ToArray();
    }

    private void markModified() => gameData.refreshModifiedState();
}
