using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private Control createBoolEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        CheckBox box = new()
        {
            IsChecked = JsonScalar.Bool(value),
            HorizontalAlignment = HorizontalAlignment.Left,
            VerticalAlignment = VerticalAlignment.Center,
        };
        box.IsCheckedChanged += (_, _) => changed(JsonValue.Create(box.IsChecked == true), false);
        return box;
    }

    private Control createInstanceVariableEditor(
        BlueprintVariableField field,
        JsonNode? value,
        Action<JsonNode?, bool> changed,
        IReadOnlySet<string>? excludedNames)
    {
        HashSet<string> allowedTypes = getInstanceVariableTypeFilter(field);
        IEnumerable<GameVariableDefinition> definitions = gameVariables?.Variables
            ?? Array.Empty<GameVariableDefinition>();
        IEnumerable<string> names = definitions
            .Where(definition => allowedTypes.Count == 0
                || allowedTypes.Contains(getGameVariableMetadataType(definition.Type)))
            .Select(definition => definition.Name)
            .Where(name => excludedNames is null || !excludedNames.Contains(name))
            .OrderBy(name => name, StringComparer.Ordinal);
        SearchableListPicker picker = new()
        {
            ItemsSource = names,
            PlaceholderText = LocaleService.Get("SELECT_GAME_VARIABLE"),
            SelectedValue = JsonScalar.TryGetString(value, out string current) ? current : string.Empty,
        };
        picker.SelectionChanged += (_, _) =>
        {
            string selected = picker.SelectedValue;
            if (JsonScalar.TryGetString(value, out string previous)
                && string.Equals(previous, selected, StringComparison.Ordinal))
            {
                return;
            }
            value = JsonValue.Create(selected);
            changed((value)?.DeepClone(), true);
        };
        return picker;
    }

    private Control createIntegerEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            getDecimal(value),
            long.MinValue,
            long.MaxValue,
            1);
        box.FormatString = "0";
        attachHistory(box);
        decimal? displayed = box.Value;
        box.ValueChanged += (_, _) =>
        {
            if (displayed == box.Value)
                return;
            displayed = box.Value;
            changed(JsonValue.Create(decimal.ToInt64(box.Value ?? 0)), false);
        };
        return box;
    }

    private Control createFloatEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        return createFloatingPointEditor(value, double.MaxValue, changed);
    }

    private Control createFloatingPointEditor(JsonNode? value, double limit, Action<JsonNode?, bool> changed)
    {
        double number = getDouble(value);
        decimal displayedNumber = getDecimal(value);
        if (double.IsFinite(number) && decimal.ToDouble(displayedNumber) == number)
        {
            NumericUpDown box = EditorInputs.CreateNumericUpDown(displayedNumber, decimal.MinValue, decimal.MaxValue, 0.1m);
            box.FormatString = "G";
            attachHistory(box);
            decimal? displayed = box.Value;
            box.ValueChanged += (_, _) =>
            {
                if (displayed == box.Value)
                    return;
                displayed = box.Value;
                double next = decimal.ToDouble(box.Value ?? 0);
                if (Math.Abs(next) <= limit)
                    changed(JsonValue.Create(next), false);
            };
            return box;
        }
        TextBox input = EditorInputs.CreateEditableTextBox(getText(value));
        input.HorizontalAlignment = HorizontalAlignment.Stretch;
        IBrush? normalBorder = input.BorderBrush;
        attachHistory(input);
        input.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty)
                return;
            if (!double.TryParse(input.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double next)
                || !double.IsFinite(next) || Math.Abs(next) > limit)
            {
                input.BorderBrush = Brushes.OrangeRed;
                return;
            }
            input.BorderBrush = normalBorder;
            changed(JsonValue.Create(next), false);
        };
        return input;
    }

    private TextBox createTextEditor(
        JsonNode? value,
        bool emptyAsNull,
        Action<JsonNode?, bool> changed)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(getText(value));
        if (emptyAsNull)
            box.PlaceholderText = "null";
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        attachHistory(box);
        box.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty)
                return;
            string text = box.Text ?? string.Empty;
            changed(emptyAsNull && string.IsNullOrWhiteSpace(text) ? null : JsonValue.Create(text), false);
        };
        return box;
    }

    private Control createAnyEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(value is null ? "null" : getText(value));
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        box.LostFocus += (_, _) => changed(parseAnyValue(box.Text ?? string.Empty), false);
        attachHistory(box);
        box.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty)
                return;
            changed(JsonValue.Create(box.Text ?? string.Empty), false);
        };
        return box;
    }

    private static JsonNode? parseAnyValue(string text)
    {
        try
        {
            JsonNode? parsed = JsonNode.Parse(text);
            if (parsed is JsonValue scalar
                && scalar.TryGetValue(out string? _))
            {
                return JsonValue.Create(text);
            }
            return parsed;
        }
        catch (JsonException)
        {
            return JsonValue.Create(text);
        }
    }

    private Control createJsonTableEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        string text = value is null
            ? "null"
            : value is JsonArray array
                ? array.ToJsonString()
                : "[]";
        TextBox box = EditorInputs.CreateEditableTextBox(text);
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        box.MinWidth = 220;
        IBrush? normalBorder = box.BorderBrush;
        attachHistory(box);
        box.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty)
                return;
            string current = box.Text ?? string.Empty;
            try
            {
                JsonNode? parsed = JsonNode.Parse(current);
                if (parsed is not null and not JsonArray)
                {
                    box.BorderBrush = new SolidColorBrush(Color.Parse("#c65353"));
                    return;
                }
                box.BorderBrush = normalBorder;
                changed(parsed, false);
            }
            catch (JsonException)
            {
                box.BorderBrush = new SolidColorBrush(Color.Parse("#c65353"));
            }
        };
        return box;
    }

    private Control createOptionEditor(
        IReadOnlyList<BlueprintVariableOption> sourceOptions,
        JsonNode? value,
        Action<JsonNode?, bool> changed)
    {
        List<BlueprintVariableOption> options = sourceOptions.Select(option => option.Clone()).ToList();
        BlueprintVariableOption? selected = options.FirstOrDefault(option => valuesSemanticallyEqual(option.Value, value));
        if (selected is null && value is not null)
        {
            selected = new BlueprintVariableOption(getText(value), value);
            options.Insert(0, selected);
        }
        ComboBox box = new()
        {
            ItemsSource = options,
            SelectedItem = selected,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            MinWidth = 180,
        };
        box.SelectionChanged += (_, _) =>
        {
            if (box.SelectedItem is BlueprintVariableOption option && !JsonNode.DeepEquals(value, option.Value))
            {
                value = (option.Value)?.DeepClone();
                changed((value)?.DeepClone(), false);
            }
        };
        return box;
    }

    private Control createVectorEditor(
        JsonNode? value,
        VectorSpec spec,
        Action<JsonNode?, bool> changed)
    {
        JsonArray source = flattenArray(value);
        Grid grid = new()
        {
            ColumnSpacing = 4,
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        for (int index = 0; index < spec.Count; index++)
        {
            int componentIndex = index;
            grid.ColumnDefinitions.Add(new ColumnDefinition(GridLength.Star));
            JsonNode? component = index < source.Count ? source[index] : null;
            void setComponent(JsonNode? next, bool refresh)
            {
                if (componentIndex < source.Count && JsonNode.DeepEquals(source[componentIndex], next))
                    return;
                while (source.Count < spec.Count)
                    source.Add(0);
                source[componentIndex] = (next)?.DeepClone();
                changed(source.DeepClone(), refresh);
            }
            Control editor;
            if (spec.IsInteger)
            {
                NumericUpDown box = EditorInputs.CreateNumericUpDown(getDecimal(component), spec.Minimum, spec.Maximum, 1);
                box.FormatString = "0";
                attachHistory(box);
                decimal? displayed = box.Value;
                box.ValueChanged += (_, _) =>
                {
                    if (displayed == box.Value)
                        return;
                    displayed = box.Value;
                    setComponent(JsonValue.Create(decimal.ToInt64(box.Value ?? 0)), false);
                };
                editor = box;
            }
            else
            {
                editor = createFloatingPointEditor(component, float.MaxValue, setComponent);
            }
            Grid.SetColumn(editor, index);
            grid.Children.Add(editor);
        }
        return grid;
    }

    private Control createIntRectEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        RectRangeSelection initial = parseRect(value, new RectRangeSelection(0, 0, CellSize, CellSize));
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,Auto,Auto,Auto"),
            ColumnSpacing = 4,
            HorizontalAlignment = HorizontalAlignment.Left,
        };
        int[] numbers = [initial.X, initial.Y, initial.Width, initial.Height];
        List<NumericUpDown> boxes = [];
        for (int index = 0; index < numbers.Length; index++)
        {
            NumericUpDown box = EditorInputs.CreateNumericUpDown(
                numbers[index],
                int.MinValue,
                int.MaxValue,
                1,
                stretch: false);
            box.FormatString = "0";
            attachHistory(box);
            boxes.Add(box);
            Grid.SetColumn(box, index);
            grid.Children.Add(box);
        }
        foreach (NumericUpDown box in boxes)
        {
            decimal? displayed = box.Value;
            box.ValueChanged += (_, _) =>
            {
                if (displayed == box.Value)
                    return;
                displayed = box.Value;
                RectRangeSelection result = new(
                    decimal.ToInt32(boxes[0].Value ?? 0),
                    decimal.ToInt32(boxes[1].Value ?? 0),
                    decimal.ToInt32(boxes[2].Value ?? 0),
                    decimal.ToInt32(boxes[3].Value ?? 0));
                changed(rectToJson(result), false);
            };
        }
        return grid;
    }

    private Control createProgressEditor(
        BlueprintVariableField field,
        JsonNode? value,
        Action<JsonNode?, bool> changed)
    {
        BlueprintVariableRange range = getProgressRange(field);
        bool returnInteger = isIntegerType(getTypeName(field), value)
            && isWhole(range.Minimum)
            && isWhole(range.Maximum)
            && isWhole(range.Step);
        BlueprintProgressEditor editor = new(value, range, returnInteger);
        attachHistory(editor.NumberInput);
        editor.ValueChanged += (_, args) => changed(args.Value, false);
        return editor;
    }

    private Control createColourEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        Color initial = parseColour(value);
        BlueprintColourSwatch swatch = new(initial);
        swatch.Click += async (_, _) =>
        {
            Window? owner = TopLevel.GetTopLevel(this) as Window;
            if (owner is null)
                return;
            Color? result = await ColourPickerWindow.ShowAsync(owner, swatch.Colour);
            if (result is not Color colour)
                return;
            swatch.Colour = colour;
            changed(new JsonArray(colour.R, colour.G, colour.B, colour.A), false);
        };
        return swatch;
    }

    private Control createPathEditor(
        BlueprintVariableField field,
        JsonNode? value,
        string assetSubdirectory,
        Action<JsonNode?, bool> changed)
    {
        TextBox box = EditorInputs.CreateReadOnlyTextBox(getText(value));
        Button browse = new()
        {
            Content = "...",
            Width = 24,
            MinWidth = 24,
            Padding = new Thickness(0),
        };
        browse.Click += async (_, _) =>
        {
            Window? owner = TopLevel.GetTopLevel(this) as Window;
            if (owner is null)
                return;
            bool projectRoot = string.Equals(
                getMetadataString(field, "PathRoot"),
                "Project",
                StringComparison.OrdinalIgnoreCase);
            bool dataRoot = string.Equals(
                getMetadataString(field, "PathRoot"),
                "Data",
                StringComparison.OrdinalIgnoreCase);
            string baseDirectory;
            if (projectRoot)
            {
                baseDirectory = getSafeDirectory(ProjectDirectory, assetSubdirectory);
            }
            else if (dataRoot)
            {
                if (!GameDataPath.TryResolveSelectionDirectory(getProjectDirectory(), assetSubdirectory, out baseDirectory))
                    return;
            }
            else if (!GameAssetPath.TryResolveSelectionDirectory(
                         getProjectDirectory(),
                         assetSubdirectory,
                         out baseDirectory))
            {
                return;
            }
            if (!Directory.Exists(baseDirectory))
            {
                if (dataRoot)
                    return;
                string fallback = projectRoot ? ProjectDirectory : AssetsDirectory;
                baseDirectory = Directory.Exists(fallback) ? fallback : Environment.CurrentDirectory;
            }
            string? pathFilter = getMetadataString(field, "PathFilter");
            string current = box.Text ?? string.Empty;
            string? initialFilePath;
            if (projectRoot)
            {
                initialFilePath = string.IsNullOrWhiteSpace(current)
                    ? null
                    : Path.Combine(baseDirectory, current);
            }
            else if (dataRoot)
            {
                initialFilePath = GameDataPath.IsCanonical(current, assetSubdirectory)
                    && GameDataPath.TryResolveExistingFile(getProjectDirectory(), current, out string resolvedData)
                    ? resolvedData
                    : null;
            }
            else
            {
                initialFilePath = GameAssetPath.TryResolveExistingFile(
                    getProjectDirectory(),
                    current,
                    out string resolvedCurrent)
                    ? resolvedCurrent
                    : null;
            }
            string? selected = await FileSelectorDialog.ShowAsync(
                owner,
                baseDirectory,
                string.IsNullOrWhiteSpace(pathFilter)
                    ? FileSelectorDialog.AllFilesFilter(star: true)
                    : FileSelectorDialog.FilesFilter(pathFilter),
                initialFilePath: initialFilePath);
            if (string.IsNullOrWhiteSpace(selected))
                return;
            string storedPath;
            if (projectRoot)
            {
                try
                {
                    storedPath = Path.GetRelativePath(baseDirectory, selected).Replace('\\', '/');
                }
                catch (ArgumentException)
                {
                    return;
                }
            }
            else if (dataRoot)
            {
                if (!GameDataPath.TryFromProjectFile(getProjectDirectory(), selected, out storedPath)
                    || !GameDataPath.IsCanonical(storedPath, assetSubdirectory))
                    return;
            }
            else if (!GameAssetPath.TryFromProjectFile(
                         getProjectDirectory(),
                         selected,
                         out storedPath))
            {
                return;
            }
            box.Text = storedPath;
            changed(JsonValue.Create(storedPath), false);
        };
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 4,
        };
        grid.Children.Add(box);
        Grid.SetColumn(browse, 1);
        grid.Children.Add(browse);
        return grid;
    }

    private Control createRectRangeEditor(
        BlueprintVariableField field,
        JsonNode? value,
        Action<JsonNode?, bool> changed)
    {
        RectRangeSelection initial = parseRect(value, new RectRangeSelection(0, 0, CellSize, CellSize));
        TextBox box = EditorInputs.CreateReadOnlyTextBox(formatRect(initial));
        Button browse = new()
        {
            Content = "...",
            Width = 24,
            MinWidth = 24,
            Padding = new Thickness(0),
        };
        browse.Click += async (_, _) =>
        {
            string? sourceFieldName = getRectSourceField(field);
            if (string.IsNullOrWhiteSpace(sourceFieldName))
                return;
            BlueprintVariableField? sourceField = fields.FirstOrDefault(
                item => string.Equals(item.Name, sourceFieldName, StringComparison.Ordinal));
            if (sourceField is null || !values.TryGetValue(sourceFieldName, out JsonNode? sourceValue))
                return;
            string relativePath = getText(sourceValue);
            if (string.IsNullOrWhiteSpace(relativePath))
                return;
            if (!GameAssetPath.TryResolveExistingFile(
                    getProjectDirectory(),
                    relativePath,
                    out string imagePath))
            {
                return;
            }
            Window? owner = TopLevel.GetTopLevel(this) as Window;
            if (owner is null)
                return;
            RectRangeSelection? result = await RectRangeWindow.ShowAsync(
                owner,
                imagePath,
                parseRect(field.Value ?? field.DefaultValue, initial),
                Math.Max(1, CellSize / 2));
            if (result is not RectRangeSelection rect)
                return;
            box.Text = formatRect(rect);
            changed(rectToJson(rect), true);
        };
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            ColumnSpacing = 4,
        };
        grid.Children.Add(box);
        Grid.SetColumn(browse, 1);
        grid.Children.Add(browse);
        return grid;
    }

    private string getProjectDirectory()
    {
        return string.IsNullOrWhiteSpace(ProjectDirectory)
            ? Path.GetDirectoryName(AssetsDirectory) ?? string.Empty
            : ProjectDirectory;
    }

    private static string getSafeDirectory(string rootDirectory, string subdirectory)
    {
        string root = string.IsNullOrWhiteSpace(rootDirectory)
            ? Environment.CurrentDirectory
            : Path.GetFullPath(rootDirectory);
        string normalized = subdirectory.Replace('\\', '/').Trim('/');
        if (string.IsNullOrWhiteSpace(normalized) || normalized == ".")
            return root;
        string candidate = Path.GetFullPath(Path.Combine(root, normalized));
        string relative = Path.GetRelativePath(root, candidate);
        if (relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative))
            return root;
        return candidate;
    }
}
