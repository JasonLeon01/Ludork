using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
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

public sealed class BlueprintVariableForm : UserControl, IDisposable
{
    private const double CompactDictionaryEntryWidth = 300;
    private readonly Grid form = new()
    {
        ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"),
        ColumnSpacing = 4,
        RowSpacing = 4,
    };
    private readonly List<BlueprintVariableField> fields = [];
    private readonly Dictionary<string, JsonNode?> values = new(StringComparer.Ordinal);
    private readonly Dictionary<string, JsonNode?> contextValues = new(StringComparer.Ordinal);
    private readonly Dictionary<string, BlueprintVariableRow> rows = new(StringComparer.Ordinal);
    private readonly HashSet<string> dependencySources = new(StringComparer.Ordinal);
    private readonly HashSet<string> instanceVariableSources = new(StringComparer.Ordinal);
    private readonly List<Control> historyControls = [];
    private readonly List<BlueprintVariableForm> nestedHistoryForms = [];
    private string assetsDirectory = string.Empty;
    private string projectDirectory = string.Empty;
    private GameDataService? historyGameData;
    private IGameVariableCatalog? gameVariables;
    private int cellSize = 32;
    private bool isReadOnly;
    private bool showFieldNames = true;
    private bool building;
    private bool gameVariablesSubscribed;
    private bool disposed;
    private long gameVariableCatalogRevision;
    private int editorRefreshGeneration;

    public BlueprintVariableForm()
    {
        Content = form;
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        if (disposed)
            return;
        subscribeGameVariables();
        long catalogRevision = gameVariables?.Revision ?? 0;
        if (gameVariableCatalogRevision != catalogRevision)
        {
            gameVariableCatalogRevision = catalogRevision;
            queueRefreshEditors(gameVariables);
        }
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        unsubscribeGameVariables();
        base.OnDetachedFromVisualTree(args);
    }

    public event EventHandler<BlueprintVariableValueChangedEventArgs>? ValueChanged;
    public event EventHandler<BlueprintComponentFieldsEventArgs>? ComponentAddRequested;
    public event EventHandler<BlueprintComponentFieldEventArgs>? ComponentRemoveRequested;

    public IReadOnlyList<BlueprintVariableField> Fields => fields;

    public Func<BlueprintVariableField, Control?>? FieldActionFactory { get; set; }
    public Func<BlueprintVariableEditorRequest, Control?>? CustomValueEditorFactory { get; set; }
    public Func<BlueprintVariableField, bool>? CanRemoveComponent { get; set; }

    public IGameVariableCatalog? GameVariables
    {
        get => gameVariables;
        set
        {
            if (ReferenceEquals(gameVariables, value))
                return;
            unsubscribeGameVariables();
            gameVariables = value;
            gameVariableCatalogRevision = gameVariables?.Revision ?? 0;
            if (disposed)
                return;
            subscribeGameVariables();
            RefreshEditors();
        }
    }

    public GameDataService? HistoryGameData
    {
        get => historyGameData;
        set
        {
            if (ReferenceEquals(historyGameData, value))
                return;
            historyGameData = value;
            if (historyGameData is null)
                return;
            foreach (Control control in historyControls)
            {
                if (control is TextBox text)
                    HistoryMergeBehavior.Attach(text, historyGameData);
                else if (control is NumericUpDown number)
                    HistoryMergeBehavior.Attach(number, historyGameData);
            }
            foreach (BlueprintVariableForm nested in nestedHistoryForms)
                nested.HistoryGameData = historyGameData;
        }
    }

    public string AssetsDirectory
    {
        get => assetsDirectory;
        set => assetsDirectory = value ?? string.Empty;
    }

    public string ProjectDirectory
    {
        get => projectDirectory;
        set => projectDirectory = value ?? string.Empty;
    }

    public int CellSize
    {
        get => cellSize;
        set => cellSize = Math.Max(1, value);
    }

    public bool IsReadOnly
    {
        get => isReadOnly;
        set
        {
            if (isReadOnly == value)
                return;
            isReadOnly = value;
            refreshDependencyStates();
        }
    }

    public bool ShowFieldNames
    {
        get => showFieldNames;
        set => showFieldNames = value;
    }

    public void SetFields(IEnumerable<BlueprintVariableField> nextFields)
    {
        if (disposed)
            return;
        editorRefreshGeneration++;
        building = true;
        disposeNestedForms();
        fields.Clear();
        fields.AddRange(nextFields.Select(field => field.Clone()));
        values.Clear();
        rows.Clear();
        dependencySources.Clear();
        instanceVariableSources.Clear();
        historyControls.Clear();
        form.Children.Clear();
        form.RowDefinitions.Clear();

        foreach (BlueprintVariableField field in fields)
        {
            values[field.Name] = cloneNode(getFieldValue(field));
            BlueprintVariableDependency? dependency = getDependency(field);
            if (dependency is not null && !string.IsNullOrWhiteSpace(dependency.Source))
                dependencySources.Add(dependency.Source);
            string? instanceVariableSource = getMetadataString(field, "InstVarValue");
            if (!string.IsNullOrWhiteSpace(instanceVariableSource))
                instanceVariableSources.Add(instanceVariableSource);
        }
        foreach (KeyValuePair<string, JsonNode?> entry in contextValues)
        {
            if (!values.ContainsKey(entry.Key))
                values[entry.Key] = cloneNode(entry.Value);
        }

        List<BlueprintVariableField> componentFields = fields
            .Where(field => field.IsComponent)
            .ToList();
        if (componentFields.Count > 0)
            addComponentRow(componentFields);

        foreach (BlueprintVariableField field in fields)
        {
            if (!field.IsComponent)
                addFieldRow(field);
        }

        building = false;
        refreshDependencyStates();
    }

    public void Clear()
    {
        contextValues.Clear();
        SetFields([]);
    }

    public void SetDependencyValue(string name, JsonNode? value)
    {
        bool hadPrevious = contextValues.TryGetValue(name, out JsonNode? previous);
        if (hadPrevious && JsonNode.DeepEquals(previous, value))
            return;
        string? previousType = hadPrevious ? resolveInstanceVariableType(previous) : null;
        contextValues[name] = cloneNode(value);
        values[name] = cloneNode(value);
        string? nextType = resolveInstanceVariableType(value);
        bool affectsInstanceVariable = instanceVariableSources.Contains(name);
        bool typeChanged = !string.Equals(previousType, nextType, StringComparison.Ordinal);
        if (affectsInstanceVariable && hadPrevious && typeChanged && nextType is not null)
        {
            resetInstanceVariableValues(name, nextType);
        }
        if (affectsInstanceVariable && typeChanged)
            RefreshEditors();
        refreshDependencyStates();
    }

    public void RefreshEditors()
    {
        if (disposed || fields.Count == 0)
            return;
        BlueprintVariableField[] nextFields = fields.Select(field => field.Clone()).ToArray();
        SetFields(nextFields);
    }

    public void SetFieldValue(string name, JsonNode? value)
    {
        BlueprintVariableField? field = fields.FirstOrDefault(
            item => string.Equals(item.Name, name, StringComparison.Ordinal));
        if (field is null || JsonNode.DeepEquals(field.Value, value))
            return;
        field.Value = cloneNode(value);
        BlueprintVariableField[] nextFields = fields.Select(item => item.Clone()).ToArray();
        SetFields(nextFields);
    }

    private void addComponentRow(IReadOnlyList<BlueprintVariableField> componentFields)
    {
        List<BlueprintVariableField> activeFields = componentFields
            .Where(field => getFieldValue(field) is not null)
            .ToList();
        List<BlueprintVariableField> addableFields = componentFields
            .Where(field => getFieldValue(field) is null)
            .ToList();
        List<ListBoxItem> items = [];
        foreach (BlueprintVariableField field in activeFields)
        {
            ListBoxItem item = new()
            {
                Content = new HintedTextPresenter
                {
                    Text = getDisplayName(field),
                },
                Tag = field,
            };
            string description = getDescription(field);
            if (!string.IsNullOrWhiteSpace(description))
                ToolTip.SetTip(item, description);
            items.Add(item);
        }

        ListBox list = new()
        {
            ItemsSource = items,
            Height = Math.Max(72, Math.Min(160, items.Count * 28 + 12)),
            HorizontalAlignment = HorizontalAlignment.Stretch,
        };
        list.DoubleTapped += async (_, _) =>
        {
            if (list.SelectedItem is not ListBoxItem item || item.Tag is not BlueprintVariableField field)
                return;
            Window? owner = TopLevel.GetTopLevel(this) as Window;
            if (owner is null)
                return;
            JsonObject? result = await BlueprintStructureWindow.ShowAsync(
                owner,
                getDisplayName(field),
                materializeStructureFields(field),
                AssetsDirectory,
                CellSize,
                GameVariables,
                isReadOnly || field.IsReadOnly);
            if (result is not null)
                commit(field, result, true);
        };
        if (items.Count > 0)
            list.SelectedIndex = 0;

        Button add = new()
        {
            Content = "+",
            Width = 24,
            Height = 34,
            Padding = new Thickness(0),
            IsEnabled = !isReadOnly && addableFields.Count > 0,
        };
        add.Click += (_, _) => ComponentAddRequested?.Invoke(
            this,
            new BlueprintComponentFieldsEventArgs(addableFields));
        Button remove = new()
        {
            Content = "-",
            Width = 24,
            Height = 34,
            Padding = new Thickness(0),
        };
        void updateRemoveState()
        {
            remove.IsEnabled = !isReadOnly
                && list.SelectedItem is ListBoxItem selected
                && selected.Tag is BlueprintVariableField field
                && (CanRemoveComponent?.Invoke(field) ?? true);
        }
        remove.Click += (_, _) =>
        {
            if (list.SelectedItem is ListBoxItem selected
                && selected.Tag is BlueprintVariableField field)
            {
                ComponentRemoveRequested?.Invoke(this, new BlueprintComponentFieldEventArgs(field));
            }
        };
        list.SelectionChanged += (_, _) => updateRemoveState();
        updateRemoveState();
        StackPanel buttons = new()
        {
            Orientation = Orientation.Vertical,
            Spacing = 4,
            Children =
            {
                add,
                remove,
            },
        };
        Grid container = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,4,24"),
        };
        container.Children.Add(list);
        Grid.SetColumn(buttons, 2);
        container.Children.Add(buttons);

        addGridRow(
            new BlueprintVariableNameLabel(LocaleService.Get("COMPONENTS"), LocaleService.Get("COMPONENTS")),
            container);
    }

    private void addFieldRow(BlueprintVariableField field)
    {
        BlueprintVariableNameLabel label = new(getDisplayName(field), field.Name);
        Control editor = createValueEditor(
            field,
            field.DisplayValue ?? getFieldValue(field),
            (value, refresh) => commit(field, value, refresh));
        string description = getDescription(field);
        BlueprintVariableDependency? dependency = getDependency(field);
        string tooltip = createTooltip(description, dependency);
        if (!string.IsNullOrWhiteSpace(tooltip))
        {
            ToolTip.SetTip(label, tooltip);
            ToolTip.SetTip(editor, tooltip);
        }
        rows[field.Name] = new BlueprintVariableRow(field, label, editor, description, dependency);
        addGridRow(label, editor, FieldActionFactory?.Invoke(field));
    }

    private void addGridRow(Control label, Control editor, Control? action = null)
    {
        int row = form.RowDefinitions.Count;
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid.SetRow(editor, row);
        if (showFieldNames)
        {
            label.VerticalAlignment = VerticalAlignment.Center;
            Grid.SetRow(label, row);
            form.Children.Add(label);
            Grid.SetColumn(editor, 1);
        }
        else
        {
            Grid.SetColumn(editor, 0);
            Grid.SetColumnSpan(editor, action is null ? 3 : 2);
        }
        form.Children.Add(editor);
        if (action is not null)
        {
            Grid.SetRow(action, row);
            Grid.SetColumn(action, 2);
            form.Children.Add(action);
        }
    }

    private Control createValueEditor(
        BlueprintVariableField field,
        JsonNode? displayValue,
        Action<JsonNode?, bool> changed,
        string? dictionaryKey = null,
        IReadOnlySet<string>? excludedInstanceVariableNames = null)
    {
        if (getMetadataNode(field, "InstVar") is not null)
        {
            return createInstanceVariableEditor(
                field,
                displayValue,
                changed,
                excludedInstanceVariableNames);
        }

        field = resolveInstanceVariableValueField(field, dictionaryKey);
        Control? customEditor = CustomValueEditorFactory?.Invoke(
            new BlueprintVariableEditorRequest(field, cloneNode(displayValue), changed));
        if (customEditor is not null)
            return customEditor;

        string? rectSource = getRectSourceField(field);
        if (!string.IsNullOrWhiteSpace(rectSource))
            return createRectRangeEditor(field, displayValue, changed);

        string? assetSubdirectory = getAssetSubdirectory(field);
        if (assetSubdirectory is not null)
            return createPathEditor(field, displayValue, assetSubdirectory, changed);

        IReadOnlyList<BlueprintVariableOption> options = getValueOptions(field);
        if (options.Count > 0)
            return createOptionEditor(options, displayValue, changed);

        if (isColourField(field))
            return createColourEditor(displayValue, changed);

        if (isProgressField(field))
            return createProgressEditor(field, displayValue, changed);

        if (field.UseJsonTableEditor)
            return createJsonTableEditor(displayValue, changed);

        string type = getTypeName(field);
        LuaMetadataType valueType = LuaMetadataType.Parse(type);
        if (valueType.Kind == LuaMetadataTypeKind.Tuple)
            return createTupleEditor(field, valueType, displayValue as JsonArray, changed, dictionaryKey);
        if (valueType.Kind == LuaMetadataTypeKind.List)
        {
            return createSequenceEditor(
                field,
                valueType.Arguments[0],
                displayValue as JsonArray,
                changed,
                dictionaryKey);
        }
        if (valueType.Kind == LuaMetadataTypeKind.Dictionary)
        {
            return createDictionaryEditor(
                field,
                valueType.Arguments[1],
                displayValue as JsonObject,
                changed,
                dictionaryKey);
        }
        if (valueType.Kind == LuaMetadataTypeKind.Table)
        {
            return displayValue is JsonArray
                ? createSequenceEditor(
                    field,
                    LuaMetadataType.Parse("any"),
                    displayValue as JsonArray,
                    changed,
                    dictionaryKey)
                : createDictionaryEditor(
                    field,
                    LuaMetadataType.Parse("any"),
                    displayValue as JsonObject,
                    changed,
                    dictionaryKey);
        }
        if (displayValue is JsonArray
            && !isPrimitiveType(type)
            && !isVectorType(field)
            && !isIntRectType(type))
        {
            return createSequenceEditor(
                field,
                LuaMetadataType.Parse("any"),
                displayValue as JsonArray,
                changed,
                dictionaryKey);
        }
        if (displayValue is JsonObject && !isPrimitiveType(type) && field.Fields.Count == 0)
        {
            return createDictionaryEditor(
                field,
                LuaMetadataType.Parse("any"),
                displayValue as JsonObject,
                changed,
                dictionaryKey);
        }

        if (field.Fields.Count > 0)
            return createInlineStructureEditor(field, changed);

        VectorSpec? vectorSpec = getVectorSpec(field);
        if (vectorSpec is VectorSpec spec)
            return createVectorEditor(displayValue, spec, changed);

        if (isIntRectType(type))
            return createIntRectEditor(displayValue, changed);

        if (valueType.IsAny
            && (displayValue is null || tryGetString(displayValue, out string _)))
        {
            return createAnyEditor(displayValue, changed);
        }

        if (isBoolType(type, displayValue))
            return createBoolEditor(displayValue, changed);

        if (isIntegerType(type, displayValue))
            return createIntegerEditor(displayValue, changed);

        if (isFloatType(type, displayValue))
            return createFloatEditor(displayValue, changed);

        return createTextEditor(
            displayValue,
            !string.Equals(type, "string", StringComparison.OrdinalIgnoreCase),
            changed);
    }

    private Control createBoolEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        CheckBox box = new()
        {
            IsChecked = getBool(value),
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
            SelectedValue = tryGetString(value, out string current) ? current : string.Empty,
        };
        picker.SelectionChanged += (_, _) =>
        {
            string selected = picker.SelectedValue;
            if (tryGetString(value, out string previous)
                && string.Equals(previous, selected, StringComparison.Ordinal))
            {
                return;
            }
            value = JsonValue.Create(selected);
            changed(cloneNode(value), true);
        };
        return picker;
    }

    private Control createIntegerEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            getDecimal(value),
            int.MinValue,
            int.MaxValue,
            1);
        box.FormatString = "0";
        attachHistory(box);
        box.ValueChanged += (_, _) => changed(JsonValue.Create(decimal.ToInt32(box.Value ?? 0)), false);
        return box;
    }

    private Control createFloatEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        NumericUpDown box = EditorInputs.CreateNumericUpDown(
            getDecimal(value),
            -999999999m,
            999999999m,
            0.1m);
        box.FormatString = "0.00";
        attachHistory(box);
        box.ValueChanged += (_, _) => changed(JsonValue.Create(decimal.ToDouble(box.Value ?? 0)), false);
        return box;
    }

    private Control createTextEditor(
        JsonNode? value,
        bool nilAsNull,
        Action<JsonNode?, bool> changed)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(formatTextValue(value, nilAsNull));
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        attachHistory(box);
        box.TextChanged += (_, _) =>
        {
            string text = box.Text ?? string.Empty;
            changed(nilAsNull && text == "nil" ? null : JsonValue.Create(text), false);
        };
        return box;
    }

    private Control createAnyEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        TextBox box = EditorInputs.CreateEditableTextBox(formatAnyValue(value));
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        attachHistory(box);
        box.TextChanged += (_, _) =>
        {
            string text = box.Text ?? string.Empty;
            changed(parseAnyValue(text), false);
        };
        return box;
    }

    private static JsonNode? parseAnyValue(string text)
    {
        if (text == "nil")
            return null;
        try
        {
            JsonNode? parsed = JsonNode.Parse(text);
            if (parsed is JsonValue scalar
                && scalar.TryGetValue(out string? _))
            {
                return JsonValue.Create(text);
            }
            return parsed ?? JsonValue.Create(text);
        }
        catch (JsonException)
        {
            return JsonValue.Create(text);
        }
    }

    private Control createJsonTableEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        string text = value is null
            ? "nil"
            : value is JsonArray array
                ? array.ToJsonString()
                : "[]";
        TextBox box = EditorInputs.CreateEditableTextBox(text);
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        box.MinWidth = 220;
        IBrush? normalBorder = box.BorderBrush;
        attachHistory(box);
        box.TextChanged += (_, _) =>
        {
            string current = box.Text ?? string.Empty;
            if (current == "nil")
            {
                box.BorderBrush = normalBorder;
                changed(null, false);
                return;
            }
            try
            {
                JsonNode? parsed = JsonNode.Parse(current);
                if (parsed is not JsonArray table)
                {
                    box.BorderBrush = new SolidColorBrush(Color.Parse("#c65353"));
                    return;
                }
                box.BorderBrush = normalBorder;
                changed(table, false);
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
            if (box.SelectedItem is BlueprintVariableOption option)
                changed(cloneNode(option.Value), false);
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
        List<NumericUpDown> boxes = [];
        for (int index = 0; index < spec.Count; index++)
        {
            grid.ColumnDefinitions.Add(new ColumnDefinition(GridLength.Star));
            decimal number = index < source.Count ? getDecimal(source[index]) : 0;
            NumericUpDown box = EditorInputs.CreateNumericUpDown(
                number,
                spec.Minimum,
                spec.Maximum,
                spec.IsInteger ? 1 : 0.1m);
            box.FormatString = spec.IsInteger ? "0" : "0.00";
            attachHistory(box);
            boxes.Add(box);
            Grid.SetColumn(box, index);
            grid.Children.Add(box);
        }
        foreach (NumericUpDown box in boxes)
        {
            box.ValueChanged += (_, _) =>
            {
                JsonArray result = [];
                foreach (NumericUpDown item in boxes)
                {
                    if (spec.IsInteger)
                        result.Add(decimal.ToInt32(item.Value ?? 0));
                    else
                        result.Add(decimal.ToDouble(item.Value ?? 0));
                }
                changed(result, false);
            };
        }
        return grid;
    }

    private Control createIntRectEditor(JsonNode? value, Action<JsonNode?, bool> changed)
    {
        RectRangeSelection initial = parseRect(value, new RectRangeSelection(0, 0, CellSize, CellSize));
        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions("86,86,86,86"),
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
            box.Width = 86;
            box.FormatString = "0";
            attachHistory(box);
            boxes.Add(box);
            Grid.SetColumn(box, index);
            grid.Children.Add(box);
        }
        foreach (NumericUpDown box in boxes)
        {
            box.ValueChanged += (_, _) =>
            {
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
            string baseDirectory = projectRoot
                ? getSafeDirectory(ProjectDirectory, assetSubdirectory)
                : getSafeAssetDirectory(assetSubdirectory);
            if (!Directory.Exists(baseDirectory))
            {
                string fallback = projectRoot ? ProjectDirectory : AssetsDirectory;
                baseDirectory = Directory.Exists(fallback) ? fallback : Environment.CurrentDirectory;
            }
            string? pathFilter = getMetadataString(field, "PathFilter");
            string current = box.Text ?? string.Empty;
            string? initialFilePath = string.IsNullOrWhiteSpace(current)
                ? null
                : Path.Combine(baseDirectory, current);
            string? selected = await FileSelectorDialog.ShowAsync(
                owner,
                baseDirectory,
                string.IsNullOrWhiteSpace(pathFilter)
                    ? FileSelectorDialog.AllFilesFilter(star: true)
                    : FileSelectorDialog.FilesFilter(pathFilter),
                initialFilePath: initialFilePath);
            if (string.IsNullOrWhiteSpace(selected))
                return;
            string relative;
            try
            {
                relative = Path.GetRelativePath(baseDirectory, selected);
            }
            catch (ArgumentException)
            {
                relative = selected;
            }
            relative = relative.Replace('\\', '/');
            box.Text = relative;
            changed(JsonValue.Create(relative), false);
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
            string baseDirectory = getSafeAssetDirectory(getAssetSubdirectory(sourceField) ?? string.Empty);
            string imagePath = Path.GetFullPath(Path.Combine(baseDirectory, relativePath));
            string imageRelative = Path.GetRelativePath(baseDirectory, imagePath);
            if (imageRelative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(imageRelative))
                return;
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

    private Control createSequenceEditor(
        BlueprintVariableField field,
        LuaMetadataType itemType,
        JsonArray? value,
        Action<JsonNode?, bool> changed,
        string? dictionaryKey)
    {
        JsonArray items = cloneNode(value) as JsonArray ?? [];
        JsonObject itemMeta = getMetadataObject(field, "ItemMeta") ?? [];
        StackPanel panel = new()
        {
            Spacing = 2,
            MinWidth = 180,
        };

        void rebuild()
        {
            panel.Children.Clear();
            for (int index = 0; index < items.Count; index++)
            {
                int itemIndex = index;
                BlueprintVariableField itemField = createContainerItemField(
                    field,
                    itemType,
                    items[itemIndex],
                    itemMeta);
                Control itemEditor = createValueEditor(
                    itemField,
                    items[itemIndex],
                    (next, refresh) =>
                    {
                        if (JsonNode.DeepEquals(items[itemIndex], next))
                            return;
                        items[itemIndex] = cloneNode(next);
                        changed(cloneNode(items), refresh);
                    },
                    dictionaryKey);
                Button remove = new()
                {
                    Content = "-",
                    Width = 24,
                    MinWidth = 24,
                    Padding = new Thickness(0),
                };
                remove.Click += (_, _) =>
                {
                    items.RemoveAt(itemIndex);
                    changed(cloneNode(items), true);
                    rebuild();
                };
                Grid row = new()
                {
                    ColumnDefinitions = new ColumnDefinitions("*,Auto"),
                    ColumnSpacing = 2,
                };
                row.Children.Add(itemEditor);
                Grid.SetColumn(remove, 1);
                row.Children.Add(remove);
                panel.Children.Add(row);
            }
            Button add = new()
            {
                Content = "+",
                Width = 24,
                MinWidth = 24,
                Padding = new Thickness(0),
                HorizontalAlignment = HorizontalAlignment.Left,
            };
            add.Click += (_, _) =>
            {
                BlueprintVariableField itemField = createContainerItemField(
                    field,
                    itemType,
                    null,
                    itemMeta);
                items.Add(createContainerDefaultNode(itemField, dictionaryKey));
                changed(cloneNode(items), true);
                rebuild();
            };
            panel.Children.Add(add);
        }

        rebuild();
        return panel;
    }

    private Control createTupleEditor(
        BlueprintVariableField field,
        LuaMetadataType tupleType,
        JsonArray? value,
        Action<JsonNode?, bool> changed,
        string? dictionaryKey)
    {
        JsonArray source = cloneNode(value) as JsonArray ?? [];
        JsonArray items = [];
        List<BlueprintVariableField> itemFields = [];
        for (int index = 0; index < tupleType.Arguments.Count; index++)
        {
            BlueprintVariableField itemField = createContainerItemField(
                field,
                tupleType.Arguments[index],
                index < source.Count ? source[index] : null,
                getTupleItemMeta(field, index));
            JsonNode? item = index < source.Count
                ? cloneNode(source[index])
                : createContainerDefaultNode(itemField, dictionaryKey);
            items.Add(item);
            itemFields.Add(itemField);
        }

        Grid grid = new()
        {
            ColumnDefinitions = new ColumnDefinitions(
                string.Join(",", Enumerable.Repeat("*", tupleType.Arguments.Count))),
            ColumnSpacing = 2,
            MinWidth = 0,
        };
        for (int index = 0; index < tupleType.Arguments.Count; index++)
        {
            int itemIndex = index;
            BlueprintVariableField itemField = itemFields[index];
            Control itemEditor = createValueEditor(
                itemField,
                items[index],
                (next, refresh) =>
                {
                    if (JsonNode.DeepEquals(items[itemIndex], next))
                        return;
                    items[itemIndex] = cloneNode(next);
                    changed(cloneNode(items), refresh);
                },
                dictionaryKey);
            itemEditor.MinWidth = 0;
            Grid.SetColumn(itemEditor, index);
            grid.Children.Add(itemEditor);
        }
        return grid;
    }

    private Control createDictionaryEditor(
        BlueprintVariableField field,
        LuaMetadataType valueType,
        JsonObject? value,
        Action<JsonNode?, bool> changed,
        string? parentDictionaryKey)
    {
        JsonObject items = cloneNode(value) as JsonObject ?? [];
        JsonObject? keyMeta = getMetadataObject(field, "DictKeyMeta");
        JsonObject itemMeta = getMetadataObject(field, "ItemMeta") ?? [];
        bool hasDraft = false;
        StackPanel panel = new()
        {
            Spacing = 2,
            MinWidth = 0,
        };

        void addEntryRow(string initialKey, JsonNode? initialValue, bool isDraft)
        {
            string currentKey = initialKey;
            BlueprintVariableField valueField = createContainerItemField(
                field,
                valueType,
                initialValue,
                itemMeta);
            JsonNode? rowValue = cloneNode(initialValue)
                ?? createContainerDefaultNode(valueField, currentKey);
            Control keyEditor;
            if (keyMeta is null)
            {
                TextBox keyBox = EditorInputs.CreateEditableTextBox(currentKey);
                attachHistory(keyBox);
                keyBox.TextChanged += (_, _) =>
                {
                    string nextKey = keyBox.Text?.Trim() ?? string.Empty;
                    if (string.Equals(nextKey, currentKey, StringComparison.Ordinal))
                        return;
                    if (!string.IsNullOrEmpty(currentKey))
                        items.Remove(currentKey);
                    currentKey = nextKey;
                    if (!string.IsNullOrEmpty(nextKey))
                        items[nextKey] = cloneNode(rowValue);
                    changed(cloneNode(items), false);
                };
                keyEditor = keyBox;
            }
            else
            {
                HashSet<string> excludedNames = new(
                    items.Select(entry => entry.Key),
                    StringComparer.Ordinal);
                excludedNames.Remove(currentKey);
                BlueprintVariableField keyField = new(string.Empty, "string", JsonValue.Create(currentKey))
                {
                    Meta = keyMeta.DeepClone() as JsonObject ?? [],
                    PreserveNullValue = false,
                };
                keyEditor = createValueEditor(
                    keyField,
                    JsonValue.Create(currentKey),
                    (next, _) =>
                    {
                        if (!tryGetString(next, out string nextKey))
                            return;
                        nextKey = nextKey.Trim();
                        if (string.Equals(nextKey, currentKey, StringComparison.Ordinal))
                            return;
                        if (nextKey.Length == 0)
                        {
                            rebuild();
                            return;
                        }
                        if (items.ContainsKey(nextKey))
                        {
                            rebuild();
                            return;
                        }

                        string? previousType = resolveInstanceVariableType(
                            JsonValue.Create(currentKey));
                        string? nextType = resolveInstanceVariableType(
                            JsonValue.Create(nextKey));
                        if (currentKey.Length > 0)
                            items.Remove(currentKey);

                        if (isDraft)
                        {
                            rowValue = createContainerDefaultNode(valueField, nextKey);
                        }
                        else if (nextType is not null
                            && !string.Equals(previousType, nextType, StringComparison.Ordinal))
                        {
                            rowValue = resetDictionaryContextValues(
                                valueField,
                                rowValue,
                                nextKey);
                        }
                        currentKey = nextKey;
                        items[currentKey] = cloneNode(rowValue);
                        if (isDraft)
                            hasDraft = false;
                        changed(cloneNode(items), false);
                        rebuild();
                    },
                    parentDictionaryKey,
                    excludedNames);
            }

            Control? valueEditor = isDraft
                ? null
                : createValueEditor(
                    valueField,
                    rowValue,
                    (next, refresh) =>
                    {
                        if (JsonNode.DeepEquals(rowValue, next))
                            return;
                        rowValue = cloneNode(next);
                        items[currentKey] = cloneNode(rowValue);
                        changed(cloneNode(items), refresh);
                    },
                    currentKey);
            Button remove = new()
            {
                Content = "-",
                Width = 24,
                MinWidth = 24,
                Padding = new Thickness(0),
            };
            remove.Click += (_, _) =>
            {
                if (isDraft)
                    hasDraft = false;
                else if (currentKey.Length > 0)
                {
                    items.Remove(currentKey);
                    changed(cloneNode(items), false);
                }
                rebuild();
            };
            Grid row = new()
            {
                ColumnDefinitions = new ColumnDefinitions("*,2*,Auto"),
                ColumnSpacing = 2,
            };
            row.Children.Add(keyEditor);
            if (valueEditor is not null)
                row.Children.Add(valueEditor);
            row.Children.Add(remove);
            bool? compactLayout = null;
            void updateEntryLayout(double width)
            {
                if (width <= 0)
                    return;
                bool nextCompact = width < CompactDictionaryEntryWidth;
                if (compactLayout == nextCompact)
                    return;
                compactLayout = nextCompact;
                row.ColumnDefinitions = nextCompact
                    ? new ColumnDefinitions("*,Auto")
                    : new ColumnDefinitions("*,2*,Auto");
                row.RowDefinitions = valueEditor is not null && nextCompact
                    ? new RowDefinitions("Auto,Auto")
                    : new RowDefinitions("Auto");
                row.RowSpacing = nextCompact ? 2 : 0;
                Grid.SetRow(keyEditor, 0);
                Grid.SetColumn(keyEditor, 0);
                Grid.SetColumnSpan(keyEditor, valueEditor is null && !nextCompact ? 2 : 1);
                if (valueEditor is not null)
                {
                    Grid.SetRow(valueEditor, nextCompact ? 1 : 0);
                    Grid.SetColumn(valueEditor, nextCompact ? 0 : 1);
                    Grid.SetColumnSpan(valueEditor, nextCompact ? 2 : 1);
                }
                Grid.SetRow(remove, 0);
                Grid.SetColumn(remove, nextCompact ? 1 : 2);
            }
            row.SizeChanged += (_, args) => updateEntryLayout(args.NewSize.Width);
            updateEntryLayout(CompactDictionaryEntryWidth);
            panel.Children.Add(row);
        }

        void rebuild()
        {
            panel.Children.Clear();
            List<KeyValuePair<string, JsonNode?>> entries = items.ToList();
            foreach (KeyValuePair<string, JsonNode?> entry in entries)
                addEntryRow(entry.Key, entry.Value, false);
            if (hasDraft)
                addEntryRow(string.Empty, null, true);
            Button add = new()
            {
                Content = "+",
                Width = 24,
                MinWidth = 24,
                Padding = new Thickness(0),
                HorizontalAlignment = HorizontalAlignment.Left,
                IsEnabled = keyMeta is null || !hasDraft,
            };
            add.Click += (_, _) =>
            {
                if (keyMeta is not null)
                {
                    hasDraft = true;
                    rebuild();
                    return;
                }
                string key = createUniqueDictionaryKey(items);
                BlueprintVariableField valueField = createContainerItemField(
                    field,
                    valueType,
                    null,
                    itemMeta);
                items[key] = createContainerDefaultNode(valueField, key);
                changed(cloneNode(items), false);
                rebuild();
            };
            panel.Children.Add(add);
        }

        rebuild();
        return panel;
    }

    private Control createInlineStructureEditor(
        BlueprintVariableField field,
        Action<JsonNode?, bool> changed)
    {
        BlueprintVariableForm nested = new()
        {
            AssetsDirectory = AssetsDirectory,
            CellSize = CellSize,
            GameVariables = GameVariables,
            IsReadOnly = isReadOnly || field.IsReadOnly,
            HistoryGameData = HistoryGameData,
        };
        nestedHistoryForms.Add(nested);
        List<BlueprintVariableField> nestedFields = materializeStructureFields(field);
        JsonObject value = buildStructureValue(nestedFields);
        nested.ValueChanged += (_, args) =>
        {
            value[args.Name] = cloneNode(args.Value);
            changed(cloneNode(value), args.RequiresRefresh);
        };
        nested.SetFields(nestedFields);
        return new Border
        {
            BorderBrush = new SolidColorBrush(Color.Parse("#444444")),
            BorderThickness = new Thickness(1),
            Padding = new Thickness(10, 5, 0, 0),
            Child = nested,
        };
    }

    private void attachHistory(TextBox control)
    {
        historyControls.Add(control);
        if (HistoryGameData is GameDataService gameData)
            HistoryMergeBehavior.Attach(control, gameData);
    }

    private void attachHistory(NumericUpDown control)
    {
        historyControls.Add(control);
        if (HistoryGameData is GameDataService gameData)
            HistoryMergeBehavior.Attach(control, gameData);
    }

    private void commit(BlueprintVariableField field, JsonNode? value, bool refresh)
    {
        values.TryGetValue(field.Name, out JsonNode? previous);
        JsonNode? next = cloneNode(value);
        field.Value = cloneNode(next);
        if (next is null)
            field.PreserveNullValue = true;
        values[field.Name] = cloneNode(next);
        refreshDependencyStates();
        if (building)
            return;
        bool requiresRefresh = refresh || dependencySources.Contains(field.Name);
        ValueChanged?.Invoke(
            this,
            new BlueprintVariableValueChangedEventArgs(field.Name, cloneNode(next), requiresRefresh));
        if (instanceVariableSources.Contains(field.Name))
        {
            string? previousType = resolveInstanceVariableType(previous);
            string? nextType = resolveInstanceVariableType(next);
            if (nextType is not null
                && !string.Equals(previousType, nextType, StringComparison.Ordinal))
            {
                resetInstanceVariableValues(field.Name, nextType);
                queueRefreshEditors();
            }
        }
    }

    private void refreshDependencyStates()
    {
        foreach (BlueprintVariableRow row in rows.Values)
        {
            bool editable = !isReadOnly && !row.Field.IsReadOnly && dependencyMatches(row.Dependency);
            row.Editor.IsEnabled = editable;
            string tooltip = createTooltip(row.Description, editable ? null : row.Dependency);
            if (!string.IsNullOrWhiteSpace(tooltip))
            {
                ToolTip.SetTip(row.Label, tooltip);
                ToolTip.SetTip(row.Editor, tooltip);
            }
        }
    }

    private bool dependencyMatches(BlueprintVariableDependency? dependency)
    {
        if (dependency is null || string.IsNullOrWhiteSpace(dependency.Source))
            return true;
        values.TryGetValue(dependency.Source, out JsonNode? actual);
        bool equal = valuesSemanticallyEqual(actual, dependency.ExpectedValue);
        return string.Equals(dependency.Operator, "!=", StringComparison.Ordinal) ? !equal : equal;
    }

    private static bool valuesSemanticallyEqual(JsonNode? left, JsonNode? right)
    {
        if (left is null || right is null)
            return left is null && right is null;
        JsonArray? leftArray = left as JsonArray;
        JsonArray? rightArray = right as JsonArray;
        if (leftArray is not null || rightArray is not null)
        {
            if (leftArray is null || rightArray is null || leftArray.Count != rightArray.Count)
                return false;
            for (int index = 0; index < leftArray.Count; index++)
            {
                if (!valuesSemanticallyEqual(leftArray[index], rightArray[index]))
                    return false;
            }
            return true;
        }
        JsonObject? leftObject = left as JsonObject;
        JsonObject? rightObject = right as JsonObject;
        if (leftObject is not null || rightObject is not null)
        {
            if (leftObject is null || rightObject is null || leftObject.Count != rightObject.Count)
                return false;
            foreach (KeyValuePair<string, JsonNode?> item in leftObject)
            {
                if (!rightObject.TryGetPropertyValue(item.Key, out JsonNode? rightValue)
                    || !valuesSemanticallyEqual(item.Value, rightValue))
                {
                    return false;
                }
            }
            return true;
        }
        bool leftIsBoolean = tryGetBoolean(left, out bool leftBoolean);
        bool rightIsBoolean = tryGetBoolean(right, out bool rightBoolean);
        if (leftIsBoolean || rightIsBoolean)
            return leftIsBoolean
                && rightIsBoolean
                && leftBoolean == rightBoolean;
        bool leftIsNumber = tryGetNumber(left, out double leftNumber);
        bool rightIsNumber = tryGetNumber(right, out double rightNumber);
        if (leftIsNumber || rightIsNumber)
            return leftIsNumber
                && rightIsNumber
                && Math.Abs(leftNumber - rightNumber) <= 0.0001;
        bool leftIsString = tryGetString(left, out string leftText);
        bool rightIsString = tryGetString(right, out string rightText);
        if (leftIsString || rightIsString)
            return leftIsString
                && rightIsString
                && string.Equals(leftText, rightText, StringComparison.Ordinal);
        return JsonNode.DeepEquals(left, right);
    }

    private static bool tryGetBoolean(JsonNode? value, out bool result)
    {
        if (value is JsonValue json && json.TryGetValue(out result))
            return true;
        result = false;
        return false;
    }

    private static bool tryGetNumber(JsonNode? value, out double result)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out double doubleValue))
            {
                result = doubleValue;
                return true;
            }
            if (json.TryGetValue(out decimal decimalValue))
            {
                result = decimal.ToDouble(decimalValue);
                return true;
            }
            if (json.TryGetValue(out long longValue))
            {
                result = longValue;
                return true;
            }
            if (json.TryGetValue(out ulong unsignedLongValue))
            {
                result = unsignedLongValue;
                return true;
            }
            if (json.TryGetValue(out int integerValue))
            {
                result = integerValue;
                return true;
            }
            if (json.TryGetValue(out uint unsignedIntegerValue))
            {
                result = unsignedIntegerValue;
                return true;
            }
            if (json.TryGetValue(out float floatValue))
            {
                result = floatValue;
                return true;
            }
            if (json.TryGetValue(out short shortValue))
            {
                result = shortValue;
                return true;
            }
            if (json.TryGetValue(out ushort unsignedShortValue))
            {
                result = unsignedShortValue;
                return true;
            }
            if (json.TryGetValue(out byte byteValue))
            {
                result = byteValue;
                return true;
            }
            if (json.TryGetValue(out sbyte signedByteValue))
            {
                result = signedByteValue;
                return true;
            }
        }
        result = 0;
        return false;
    }

    private List<BlueprintVariableField> materializeStructureFields(BlueprintVariableField field)
    {
        JsonObject value = getFieldValue(field) as JsonObject ?? [];
        if (field.Fields.Count > 0)
        {
            List<BlueprintVariableField> result = [];
            foreach (BlueprintVariableField definition in field.Fields)
            {
                BlueprintVariableField child = definition.Clone();
                if (value.TryGetPropertyValue(child.Name, out JsonNode? childValue))
                {
                    child.Value = cloneNode(childValue);
                    child.PreserveNullValue = childValue is null;
                }
                else if (child.Value is null)
                {
                    child.Value = cloneNode(child.DefaultValue);
                    child.PreserveNullValue = false;
                }
                result.Add(child);
            }
            return result;
        }

        List<BlueprintVariableField> inferred = [];
        foreach (KeyValuePair<string, JsonNode?> item in value)
            inferred.Add(new BlueprintVariableField(item.Key, inferType(item.Value), item.Value));
        return inferred;
    }

    private static JsonObject buildStructureValue(IEnumerable<BlueprintVariableField> structureFields)
    {
        JsonObject result = [];
        foreach (BlueprintVariableField field in structureFields)
            result[field.Name] = cloneNode(getFieldValue(field));
        return result;
    }

    private BlueprintVariableField createContainerItemField(
        BlueprintVariableField field,
        LuaMetadataType itemType,
        JsonNode? value,
        JsonObject meta)
    {
        string typeName = itemType.ToString();
        LuaTypeReference? reference = itemType.Kind == LuaMetadataTypeKind.Named
            ? LuaTypeReference.Parse(typeName)
            : null;
        return new BlueprintVariableField(string.Empty, typeName, value)
        {
            Module = reference?.ModuleName,
            TypeName = reference?.TypeName ?? typeName,
            Fields = field.Fields.Select(item => item.Clone()).ToArray(),
            Meta = meta.DeepClone() as JsonObject ?? [],
            PreserveNullValue = value is null,
        };
    }

    private JsonNode? createContainerDefaultNode(
        BlueprintVariableField field,
        string? dictionaryKey)
    {
        BlueprintVariableField resolvedField = resolveInstanceVariableValueField(field, dictionaryKey);
        IReadOnlyList<BlueprintVariableOption> options = getValueOptions(resolvedField);
        if (options.Count > 0)
            return cloneNode(options[0].Value);

        LuaMetadataType type = LuaMetadataType.Parse(getTypeName(resolvedField));
        if (type.Kind == LuaMetadataTypeKind.Tuple)
        {
            JsonArray tuple = [];
            for (int index = 0; index < type.Arguments.Count; index++)
            {
                BlueprintVariableField itemField = createContainerItemField(
                    resolvedField,
                    type.Arguments[index],
                    null,
                    getTupleItemMeta(resolvedField, index));
                tuple.Add(createContainerDefaultNode(itemField, dictionaryKey));
            }
            return tuple;
        }
        if (type.Kind == LuaMetadataTypeKind.List)
            return new JsonArray();
        if (type.Kind is LuaMetadataTypeKind.Dictionary or LuaMetadataTypeKind.Table)
            return new JsonObject();
        return createDefaultNode(type, resolvedField.Fields);
    }

    private JsonNode? resetDictionaryContextValues(
        BlueprintVariableField field,
        JsonNode? value,
        string dictionaryKey)
    {
        if (string.Equals(
                getMetadataString(field, "InstVarValue"),
                "$dictKey",
                StringComparison.Ordinal))
        {
            return createContainerDefaultNode(field, dictionaryKey);
        }

        LuaMetadataType type = LuaMetadataType.Parse(getTypeName(field));
        if (type.Kind == LuaMetadataTypeKind.Tuple)
        {
            JsonArray source = cloneNode(value) as JsonArray ?? [];
            JsonArray result = [];
            for (int index = 0; index < type.Arguments.Count; index++)
            {
                BlueprintVariableField itemField = createContainerItemField(
                    field,
                    type.Arguments[index],
                    index < source.Count ? source[index] : null,
                    getTupleItemMeta(field, index));
                JsonNode? item = index < source.Count
                    ? resetDictionaryContextValues(itemField, source[index], dictionaryKey)
                    : createContainerDefaultNode(itemField, dictionaryKey);
                result.Add(item);
            }
            return result;
        }
        if (type.Kind == LuaMetadataTypeKind.List)
        {
            JsonArray source = cloneNode(value) as JsonArray ?? [];
            JsonArray result = [];
            JsonObject itemMeta = getMetadataObject(field, "ItemMeta") ?? [];
            foreach (JsonNode? item in source)
            {
                BlueprintVariableField itemField = createContainerItemField(
                    field,
                    type.Arguments[0],
                    item,
                    itemMeta);
                result.Add(resetDictionaryContextValues(itemField, item, dictionaryKey));
            }
            return result;
        }
        return cloneNode(value);
    }

    private static JsonNode? createDefaultNode(
        LuaMetadataType type,
        IReadOnlyList<BlueprintVariableField> structureFields)
    {
        if (structureFields.Count > 0)
            return buildStructureValue(structureFields);
        return LuaMetadataValueDefaults.Create(
            type,
            _ => JsonValue.Create(string.Empty));
    }

    private BlueprintVariableField resolveInstanceVariableValueField(
        BlueprintVariableField field,
        string? dictionaryKey = null)
    {
        string? source = getMetadataString(field, "InstVarValue");
        if (string.IsNullOrWhiteSpace(source))
            return field;
        JsonNode? sourceValue;
        if (string.Equals(source, "$dictKey", StringComparison.Ordinal))
        {
            if (string.IsNullOrEmpty(dictionaryKey))
                return field;
            sourceValue = JsonValue.Create(dictionaryKey);
        }
        else if (!values.TryGetValue(source, out sourceValue))
        {
            return field;
        }
        string? type = resolveInstanceVariableType(sourceValue);
        return type is null ? field : cloneFieldWithType(field, type);
    }

    private string? resolveInstanceVariableType(JsonNode? value)
    {
        if (gameVariables is null
            || !tryGetString(value, out string name)
            || !gameVariables.TryGet(name, out GameVariableDefinition? definition)
            || definition is null)
        {
            return null;
        }
        return getGameVariableMetadataType(definition.Type);
    }

    private void resetInstanceVariableValues(string source, string type)
    {
        foreach (BlueprintVariableField field in fields)
        {
            if (!string.Equals(
                    getMetadataString(field, "InstVarValue"),
                    source,
                    StringComparison.Ordinal))
            {
                continue;
            }
            JsonNode? value = createDefaultNode(LuaMetadataType.Parse(type), field.Fields);
            field.Value = cloneNode(value);
            field.PreserveNullValue = value is null;
            values[field.Name] = cloneNode(value);
            if (!building)
            {
                ValueChanged?.Invoke(
                    this,
                    new BlueprintVariableValueChangedEventArgs(field.Name, cloneNode(value), true));
            }
        }
    }

    private static BlueprintVariableField cloneFieldWithType(
        BlueprintVariableField field,
        string type)
    {
        return new BlueprintVariableField(field.Name, type, field.Value)
        {
            Description = field.Description,
            Module = null,
            TypeName = type,
            DefaultValue = field.DefaultValue?.DeepClone(),
            DisplayValue = field.DisplayValue?.DeepClone(),
            Meta = field.Meta.DeepClone() as JsonObject ?? [],
            IsComponent = field.IsComponent,
            IsReadOnly = field.IsReadOnly,
            UseJsonTableEditor = field.UseJsonTableEditor,
            PreserveNullValue = field.PreserveNullValue,
            EditorKind = field.EditorKind,
            RelatedFieldName = field.RelatedFieldName,
            AssetSubdirectory = field.AssetSubdirectory,
            RectSourceField = field.RectSourceField,
            Dependency = field.Dependency?.Clone(),
            Range = field.Range,
            Options = field.Options.Select(option => option.Clone()).ToArray(),
            Fields = field.Fields.Select(item => item.Clone()).ToArray(),
        };
    }

    private static HashSet<string> getInstanceVariableTypeFilter(BlueprintVariableField field)
    {
        HashSet<string> result = new(StringComparer.OrdinalIgnoreCase);
        if (getMetadataNode(field, "InstVar") is not JsonObject config
            || config["types"] is not JsonArray types)
        {
            return result;
        }
        foreach (JsonNode? item in types)
        {
            if (tryGetString(item, out string type) && !string.IsNullOrWhiteSpace(type))
                result.Add(type.Trim());
        }
        return result;
    }

    private static string getGameVariableMetadataType(GameVariableType type)
    {
        return type switch
        {
            GameVariableType.Bool => "bool",
            GameVariableType.Int => "int",
            GameVariableType.Float => "float",
            GameVariableType.String => "string",
            GameVariableType.List => "any[]",
            GameVariableType.Dict => "Dict[string, any]",
            _ => throw new ArgumentOutOfRangeException(nameof(type)),
        };
    }

    private static string createUniqueDictionaryKey(JsonObject items)
    {
        string key = "key";
        int suffix = 2;
        while (items.ContainsKey(key))
        {
            key = $"key_{suffix}";
            suffix++;
        }
        return key;
    }

    private string getSafeAssetDirectory(string subdirectory)
    {
        return getSafeDirectory(AssetsDirectory, subdirectory);
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

    private static string getDisplayName(BlueprintVariableField field)
    {
        return EditorDisplayName.Format(field.Name);
    }

    private static string getDescription(BlueprintVariableField field)
    {
        return field.Description;
    }

    private static string createTooltip(
        string description,
        BlueprintVariableDependency? dependency)
    {
        List<string> parts = [];
        if (!string.IsNullOrWhiteSpace(description))
            parts.Add(description);
        if (dependency is not null && !string.IsNullOrWhiteSpace(dependency.Source))
        {
            string expected = formatNode(dependency.ExpectedValue);
            if (string.Equals(dependency.Operator, "!=", StringComparison.Ordinal))
                expected = $"!= {expected}";
            string template = LocaleService.Get("META_RELY_TOOLTIP");
            parts.Add(template
                .Replace("{source}", dependency.Source, StringComparison.Ordinal)
                .Replace("{value}", expected, StringComparison.Ordinal));
        }
        return string.Join("\n\n", parts);
    }

    private static string? getAssetSubdirectory(BlueprintVariableField field)
    {
        if (field.AssetSubdirectory is not null)
            return normalizeAssetSubdirectory(field.AssetSubdirectory);
        string? value = getMetadataString(field, "PathVars");
        return value is null ? null : normalizeAssetSubdirectory(value);
    }

    private static string normalizeAssetSubdirectory(string value)
    {
        return value.Replace('\\', '/').Trim('/');
    }

    private static string? getRectSourceField(BlueprintVariableField field)
    {
        if (!string.IsNullOrWhiteSpace(field.RectSourceField))
            return field.RectSourceField;
        return getMetadataString(field, "RectRangeVars");
    }

    private static BlueprintVariableDependency? getDependency(BlueprintVariableField field)
    {
        if (field.Dependency is not null)
            return field.Dependency.Clone();
        JsonNode? rely = getMetadataNode(field, "Rely");
        if (rely is JsonObject relyMap
            && getObjectString(relyMap, "source") is null
            && getObjectString(relyMap, "key") is null
            && getObjectString(relyMap, "var") is null
            && relyMap.TryGetPropertyValue(field.Name, out JsonNode? nestedRule)
            && nestedRule is not null)
        {
            rely = nestedRule;
        }
        if (rely is JsonArray array
            && array.Count >= 2
            && tryGetString(array[0], out string source))
        {
            return new BlueprintVariableDependency(source, cloneNode(array[1]));
        }
        if (rely is not JsonObject rule)
            return null;
        string? sourceName = getObjectString(rule, "source")
            ?? getObjectString(rule, "key")
            ?? getObjectString(rule, "var");
        if (string.IsNullOrWhiteSpace(sourceName))
            return null;
        string operation = getObjectString(rule, "op")
            ?? getObjectString(rule, "operator")
            ?? "==";
        rule.TryGetPropertyValue("value", out JsonNode? expected);
        return new BlueprintVariableDependency(sourceName, cloneNode(expected), operation);
    }

    private static bool isColourField(BlueprintVariableField field)
    {
        string type = getTypeName(field);
        return string.Equals(type, "sf.Color", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Color", StringComparison.OrdinalIgnoreCase);
    }

    private static bool isProgressField(BlueprintVariableField field)
    {
        return field.Range is not null
            || getMetadataNode(field, "ProgressVars") is not null
            || getMetadataNode(field, "SliderVars") is not null
            || getMetadataNode(field, "RangeVars") is not null;
    }

    private static BlueprintVariableRange getProgressRange(BlueprintVariableField field)
    {
        if (field.Range is not null)
            return field.Range.Normalize();
        JsonNode? spec = getMetadataNode(field, "ProgressVars")
            ?? getMetadataNode(field, "SliderVars")
            ?? getMetadataNode(field, "RangeVars");
        if (spec is JsonValue)
            return new BlueprintVariableRange(0, getDouble(spec, 100), 1).Normalize();
        if (spec is JsonArray array)
        {
            double minimum = array.Count > 0 ? getDouble(array[0], 0) : 0;
            double maximum = array.Count > 1 ? getDouble(array[1], 100) : 100;
            double step = array.Count > 2 ? getDouble(array[2], 1) : 1;
            return new BlueprintVariableRange(minimum, maximum, step).Normalize();
        }
        if (spec is JsonObject range)
        {
            double minimum = getDouble(range["minimum"] ?? range["min"], 0);
            double maximum = getDouble(range["maximum"] ?? range["max"], 100);
            double step = getDouble(range["step"], 1);
            return new BlueprintVariableRange(minimum, maximum, step).Normalize();
        }
        return new BlueprintVariableRange(0, 100, 1);
    }

    private static VectorSpec? getVectorSpec(BlueprintVariableField field)
    {
        string type = getTypeName(field);
        if (string.Equals(type, "Pair", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "sf.Vector2f", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector2f", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(2, false, -999999999m, 999999999m);
        if (string.Equals(type, "sf.Vector2i", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector2i", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(2, true, int.MinValue, int.MaxValue);
        if (string.Equals(type, "sf.Vector2u", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector2u", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(2, true, 0, int.MaxValue);
        if (string.Equals(type, "sf.Vector3f", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3f", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, false, -999999999m, 999999999m);
        if (string.Equals(type, "sf.Vector3i", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3i", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, true, int.MinValue, int.MaxValue);
        if (string.Equals(type, "sf.Vector3u", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3u", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, true, 0, int.MaxValue);
        return null;
    }

    private static bool isVectorType(BlueprintVariableField field)
    {
        return getVectorSpec(field) is not null;
    }

    private static bool isIntRectType(string type)
    {
        return string.Equals(type, "sf.IntRect", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "IntRect", StringComparison.OrdinalIgnoreCase);
    }

    private static bool isBoolType(string type, JsonNode? value)
    {
        if (string.Equals(type, "bool", StringComparison.OrdinalIgnoreCase))
            return true;
        if (isPrimitiveType(type))
            return false;
        return value is JsonValue json && json.TryGetValue(out bool _);
    }

    private static bool isIntegerType(string type, JsonNode? value)
    {
        if (string.Equals(type, "int", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        if (isPrimitiveType(type))
            return false;
        if (value is not JsonValue json)
            return false;
        return json.TryGetValue(out int _) || json.TryGetValue(out long _);
    }

    private static bool isFloatType(string type, JsonNode? value)
    {
        if (string.Equals(type, "float", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "double", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "number", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }
        if (isPrimitiveType(type))
            return false;
        if (value is not JsonValue json)
            return false;
        return json.TryGetValue(out double _) || json.TryGetValue(out decimal _);
    }

    private static bool isPrimitiveType(string type)
    {
        return string.Equals(type, "bool", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "int", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "float", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "double", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "number", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "string", StringComparison.OrdinalIgnoreCase);
    }

    private static string getTypeName(BlueprintVariableField field)
    {
        return string.IsNullOrWhiteSpace(field.TypeName) ? field.Type : field.TypeName;
    }

    private static string inferType(JsonNode? value)
    {
        if (value is JsonObject)
            return "dict";
        if (value is JsonArray)
            return "list";
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out bool _))
                return "bool";
            if (json.TryGetValue(out int _) || json.TryGetValue(out long _))
                return "int";
            if (json.TryGetValue(out double _) || json.TryGetValue(out decimal _))
                return "float";
        }
        return "string";
    }

    private static JsonArray flattenArray(JsonNode? value)
    {
        JsonArray result = [];
        if (value is not JsonArray source)
            return result;
        foreach (JsonNode? item in source)
        {
            if (item is JsonArray nested)
            {
                foreach (JsonNode? nestedItem in nested)
                    result.Add(cloneNode(nestedItem));
            }
            else
            {
                result.Add(cloneNode(item));
            }
        }
        return result;
    }

    private static RectRangeSelection parseRect(JsonNode? value, RectRangeSelection fallback)
    {
        if (value is not JsonArray values || values.Count == 0
            || values[0] is not JsonArray arguments || arguments.Count < 4)
            return fallback;
        return new RectRangeSelection(
            getInt(arguments[0], fallback.X),
            getInt(arguments[1], fallback.Y),
            getInt(arguments[2], fallback.Width),
            getInt(arguments[3], fallback.Height));
    }

    private static JsonArray rectToJson(RectRangeSelection rect)
    {
        return new JsonArray(new JsonArray(rect.X, rect.Y, rect.Width, rect.Height));
    }

    private static string formatRect(RectRangeSelection rect)
    {
        return $"(({rect.X}, {rect.Y}), ({rect.Width}, {rect.Height}))";
    }

    private static Color parseColour(JsonNode? value)
    {
        if (value is JsonValue textValue && textValue.TryGetValue(out string? text))
        {
            string candidate = text?.Trim() ?? string.Empty;
            if (candidate.StartsWith('#') && Color.TryParse(candidate, out Color parsed))
                return parsed;
        }
        JsonArray channels = flattenArray(value);
        if (channels.Count < 3)
            return Colors.White;
        byte red = (byte)Math.Clamp(getInt(channels[0], 255), 0, 255);
        byte green = (byte)Math.Clamp(getInt(channels[1], 255), 0, 255);
        byte blue = (byte)Math.Clamp(getInt(channels[2], 255), 0, 255);
        byte alpha = (byte)Math.Clamp(channels.Count > 3 ? getInt(channels[3], 255) : 255, 0, 255);
        return Color.FromArgb(alpha, red, green, blue);
    }

    private static JsonNode? getMetadataNode(BlueprintVariableField field, string key)
    {
        if (field.Meta.TryGetPropertyValue(key, out JsonNode? direct))
            return direct;
        if (field.Meta["Meta"] is JsonObject nested
            && nested.TryGetPropertyValue(key, out JsonNode? nestedValue))
        {
            return nestedValue;
        }
        return null;
    }

    private static JsonObject? getMetadataObject(BlueprintVariableField field, string key)
    {
        return getMetadataNode(field, key) is JsonObject value
            ? value.DeepClone() as JsonObject
            : null;
    }

    private static JsonObject getTupleItemMeta(BlueprintVariableField field, int index)
    {
        JsonNode? tupleMeta = getMetadataNode(field, "TupleMeta");
        if (tupleMeta is JsonArray tupleArray
            && index >= 0
            && index < tupleArray.Count
            && tupleArray[index] is JsonObject arrayItemMeta)
        {
            return arrayItemMeta.DeepClone() as JsonObject ?? [];
        }
        if (tupleMeta is JsonObject tupleObject
            && tupleObject[(index + 1).ToString(CultureInfo.InvariantCulture)] is JsonObject objectItemMeta)
        {
            return objectItemMeta.DeepClone() as JsonObject ?? [];
        }
        return [];
    }

    private static IReadOnlyList<BlueprintVariableOption> getValueOptions(
        BlueprintVariableField field)
    {
        if (field.Options.Count > 0)
            return field.Options;
        if (getMetadataNode(field, "DropBox") is not JsonArray values)
            return [];
        List<BlueprintVariableOption> result = [];
        foreach (JsonNode? value in values)
        {
            string label = tryGetString(value, out string text)
                ? text
                : value?.ToJsonString() ?? string.Empty;
            result.Add(new BlueprintVariableOption(label, value));
        }
        return result;
    }

    private static string? getMetadataString(BlueprintVariableField field, string key)
    {
        JsonNode? value = getMetadataNode(field, key);
        return tryGetString(value, out string result) ? result : null;
    }

    private static string? getObjectString(JsonObject value, string key)
    {
        return value.TryGetPropertyValue(key, out JsonNode? node)
            && tryGetString(node, out string result)
            ? result
            : null;
    }

    private static bool tryGetString(JsonNode? value, out string result)
    {
        if (value is JsonValue json && json.TryGetValue(out string? text) && text is not null)
        {
            result = text;
            return true;
        }
        result = string.Empty;
        return false;
    }

    private static string getText(JsonNode? value)
    {
        if (value is null)
            return string.Empty;
        if (tryGetString(value, out string text))
            return text;
        return value.ToJsonString();
    }

    private static string formatTextValue(JsonNode? value, bool nilAsNull)
    {
        return value is null && nilAsNull ? "nil" : getText(value);
    }

    private static string formatAnyValue(JsonNode? value)
    {
        if (value is null)
            return "nil";
        return getText(value);
    }

    private static string formatNode(JsonNode? value)
    {
        if (value is null)
            return "nil";
        if (value is JsonValue json && json.TryGetValue(out bool boolean))
            return boolean ? "true" : "false";
        return getText(value);
    }

    private static bool getBool(JsonNode? value)
    {
        return value is JsonValue json && json.TryGetValue(out bool result) && result;
    }

    private static int getInt(JsonNode? value, int fallback = 0)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out int integer))
                return integer;
            if (json.TryGetValue(out long longValue))
                return (int)Math.Clamp(longValue, int.MinValue, int.MaxValue);
            if (json.TryGetValue(out double number))
                return (int)number;
        }
        return int.TryParse(getText(value), NumberStyles.Integer, CultureInfo.InvariantCulture, out int result)
            ? result
            : fallback;
    }

    private static double getDouble(JsonNode? value, double fallback = 0)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out double number))
                return number;
            if (json.TryGetValue(out decimal decimalValue))
                return decimal.ToDouble(decimalValue);
            if (json.TryGetValue(out int integer))
                return integer;
        }
        return double.TryParse(getText(value), NumberStyles.Float, CultureInfo.InvariantCulture, out double result)
            ? result
            : fallback;
    }

    private static decimal getDecimal(JsonNode? value)
    {
        double number = getDouble(value);
        if (number > (double)decimal.MaxValue)
            return decimal.MaxValue;
        if (number < (double)decimal.MinValue)
            return decimal.MinValue;
        return (decimal)number;
    }

    private static bool isWhole(double value)
    {
        return Math.Abs(value - Math.Round(value)) < double.Epsilon;
    }

    private static JsonNode? cloneNode(JsonNode? value)
    {
        return value?.DeepClone();
    }

    private void subscribeGameVariables()
    {
        if (disposed
            || gameVariablesSubscribed
            || gameVariables is null
            || VisualRoot is null)
        {
            return;
        }
        gameVariables.Changed += onGameVariablesChanged;
        gameVariablesSubscribed = true;
    }

    private void unsubscribeGameVariables()
    {
        if (!gameVariablesSubscribed || gameVariables is null)
            return;
        gameVariables.Changed -= onGameVariablesChanged;
        gameVariablesSubscribed = false;
    }

    private void onGameVariablesChanged(object? sender, EventArgs args)
    {
        long catalogRevision = gameVariables?.Revision ?? 0;
        if (gameVariableCatalogRevision == catalogRevision)
            return;
        gameVariableCatalogRevision = catalogRevision;
        queueRefreshEditors(sender);
    }

    private void queueRefreshEditors(object? expectedCatalog = null)
    {
        if (disposed)
            return;
        int generation = ++editorRefreshGeneration;
        Dispatcher.UIThread.Post(() =>
        {
            if (disposed
                || VisualRoot is null
                || generation != editorRefreshGeneration
                || expectedCatalog is not null && !ReferenceEquals(gameVariables, expectedCatalog))
            {
                return;
            }
            RefreshEditors();
        }, DispatcherPriority.Background);
    }

    private void disposeNestedForms()
    {
        foreach (BlueprintVariableForm nested in nestedHistoryForms)
            nested.Dispose();
        nestedHistoryForms.Clear();
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        editorRefreshGeneration++;
        unsubscribeGameVariables();
        disposeNestedForms();
        gameVariables = null;
    }

    private static JsonNode? getFieldValue(BlueprintVariableField field)
    {
        return field.PreserveNullValue ? field.Value : field.Value ?? field.DefaultValue;
    }

    private sealed record BlueprintVariableRow(
        BlueprintVariableField Field,
        Control Label,
        Control Editor,
        string Description,
        BlueprintVariableDependency? Dependency);

    private readonly record struct VectorSpec(int Count, bool IsInteger, decimal Minimum, decimal Maximum);
}

public enum BlueprintVariableEditorKind
{
    Default,
    MoveRoute,
    TransferPosition,
    BlueprintClass,
    CommonFunction,
}

public sealed class BlueprintVariableField
{
    public BlueprintVariableField(string name, string type, JsonNode? value = null)
    {
        Name = name;
        Type = type;
        Value = value?.DeepClone();
    }

    public string Name { get; init; }
    public string Description { get; init; } = string.Empty;
    public string Type { get; init; }
    public string? Module { get; init; }
    public string? TypeName { get; init; }
    public JsonNode? Value { get; set; }
    public JsonNode? DefaultValue { get; init; }
    public JsonNode? DisplayValue { get; init; }
    public JsonObject Meta { get; init; } = [];
    public bool IsComponent { get; init; }
    public bool IsReadOnly { get; init; }
    public bool UseJsonTableEditor { get; init; }
    public bool PreserveNullValue { get; set; }
    public BlueprintVariableEditorKind EditorKind { get; init; }
    public string? RelatedFieldName { get; init; }
    public string? AssetSubdirectory { get; init; }
    public string? RectSourceField { get; init; }
    public BlueprintVariableDependency? Dependency { get; init; }
    public BlueprintVariableRange? Range { get; init; }
    public IReadOnlyList<BlueprintVariableOption> Options { get; init; } = [];
    public IReadOnlyList<BlueprintVariableField> Fields { get; init; } = [];

    public BlueprintVariableField Clone()
    {
        return new BlueprintVariableField(Name, Type, Value)
        {
            Description = Description,
            Module = Module,
            TypeName = TypeName,
            DefaultValue = DefaultValue?.DeepClone(),
            DisplayValue = DisplayValue?.DeepClone(),
            Meta = Meta.DeepClone() as JsonObject ?? [],
            IsComponent = IsComponent,
            IsReadOnly = IsReadOnly,
            UseJsonTableEditor = UseJsonTableEditor,
            PreserveNullValue = PreserveNullValue,
            EditorKind = EditorKind,
            RelatedFieldName = RelatedFieldName,
            AssetSubdirectory = AssetSubdirectory,
            RectSourceField = RectSourceField,
            Dependency = Dependency?.Clone(),
            Range = Range,
            Options = Options.Select(option => option.Clone()).ToArray(),
            Fields = Fields.Select(field => field.Clone()).ToArray(),
        };
    }
}

public sealed class BlueprintVariableEditorRequest
{
    public BlueprintVariableEditorRequest(
        BlueprintVariableField field,
        JsonNode? value,
        Action<JsonNode?, bool> commit)
    {
        Field = field;
        Value = value?.DeepClone();
        Commit = commit;
    }

    public BlueprintVariableField Field { get; }
    public JsonNode? Value { get; }
    public Action<JsonNode?, bool> Commit { get; }
}

public sealed class BlueprintVariableOption
{
    public BlueprintVariableOption(string label, JsonNode? value)
    {
        Label = label;
        Value = value?.DeepClone();
    }

    public string Label { get; }
    public JsonNode? Value { get; }

    public BlueprintVariableOption Clone()
    {
        return new BlueprintVariableOption(Label, Value);
    }

    public override string ToString()
    {
        return Label;
    }
}

public sealed class BlueprintVariableDependency
{
    public BlueprintVariableDependency(string source, JsonNode? expectedValue, string operation = "==")
    {
        Source = source;
        ExpectedValue = expectedValue?.DeepClone();
        Operator = operation == "!=" ? "!=" : "==";
    }

    public string Source { get; }
    public JsonNode? ExpectedValue { get; }
    public string Operator { get; }

    public BlueprintVariableDependency Clone()
    {
        return new BlueprintVariableDependency(Source, ExpectedValue, Operator);
    }
}

public sealed record BlueprintVariableRange(double Minimum, double Maximum, double Step)
{
    public BlueprintVariableRange Normalize()
    {
        double minimum = Minimum;
        double maximum = Maximum;
        if (maximum < minimum)
            (minimum, maximum) = (maximum, minimum);
        double step = Step > 0 ? Step : 1;
        return new BlueprintVariableRange(minimum, maximum, step);
    }
}

public sealed class BlueprintVariableValueChangedEventArgs(
    string name,
    JsonNode? value,
    bool requiresRefresh) : EventArgs
{
    public string Name { get; } = name;
    public JsonNode? Value { get; } = value;
    public bool RequiresRefresh { get; } = requiresRefresh;
}

public sealed class BlueprintComponentFieldsEventArgs(
    IReadOnlyList<BlueprintVariableField> fields) : EventArgs
{
    public IReadOnlyList<BlueprintVariableField> Fields { get; } = fields;
}

public sealed class BlueprintComponentFieldEventArgs(
    BlueprintVariableField field) : EventArgs
{
    public BlueprintVariableField Field { get; } = field;
}

internal sealed class BlueprintVariableNameLabel : TextBlock
{
    private readonly string displayName;
    private readonly string variableName;
    private readonly DispatcherTimer timer = new()
    {
        Interval = TimeSpan.FromSeconds(5),
    };

    public BlueprintVariableNameLabel(string display, string variable)
    {
        displayName = display;
        variableName = variable;
        Text = display;
        timer.Tick += (_, _) => showDisplayName();
        if (!string.Equals(displayName, variableName, StringComparison.Ordinal))
            Cursor = new Cursor(StandardCursorType.Hand);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (!args.GetCurrentPoint(this).Properties.IsLeftButtonPressed
            || string.Equals(displayName, variableName, StringComparison.Ordinal))
        {
            return;
        }
        if (string.Equals(Text, variableName, StringComparison.Ordinal))
        {
            showDisplayName();
        }
        else
        {
            Text = variableName;
            timer.Stop();
            timer.Start();
        }
        args.Handled = true;
    }

    private void showDisplayName()
    {
        timer.Stop();
        Text = displayName;
    }
}

internal sealed class BlueprintProgressEditor : Grid
{
    private readonly Slider slider = new();
    private readonly NumericUpDown number;
    private readonly BlueprintVariableRange range;
    private readonly bool returnInteger;
    private bool syncing;

    public BlueprintProgressEditor(JsonNode? value, BlueprintVariableRange valueRange, bool integer)
    {
        range = valueRange.Normalize();
        returnInteger = integer;
        int stepCount = Math.Max(1, (int)Math.Round((range.Maximum - range.Minimum) / range.Step));
        slider.Orientation = Orientation.Horizontal;
        slider.Minimum = 0;
        slider.Maximum = stepCount;
        slider.SmallChange = 1;
        slider.LargeChange = Math.Max(1, Math.Min(10, stepCount / 10));

        decimal minimum = toDecimal(range.Minimum);
        decimal maximum = toDecimal(range.Maximum);
        decimal increment = Math.Max(0.000001m, toDecimal(range.Step));
        number = EditorInputs.CreateNumericUpDown(0, minimum, maximum, increment, stretch: false);
        number.Width = 96;
        number.FormatString = createFormatString(range.Step);

        ColumnDefinitions = new ColumnDefinitions("*,96");
        ColumnSpacing = 6;
        Children.Add(slider);
        Grid.SetColumn(number, 1);
        Children.Add(number);

        slider.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setValue(range.Minimum + slider.Value * range.Step, true);
        };
        number.ValueChanged += (_, _) =>
        {
            if (syncing)
                return;
            setValue(decimal.ToDouble(number.Value ?? 0), true);
        };
        setValue(readDouble(value, range.Minimum), false);
    }

    public event EventHandler<BlueprintProgressValueChangedEventArgs>? ValueChanged;
    public NumericUpDown NumberInput => number;

    private void setValue(double value, bool emit)
    {
        double clamped = Math.Clamp(value, range.Minimum, range.Maximum);
        int sliderValue = Math.Clamp(
            (int)Math.Round((clamped - range.Minimum) / range.Step),
            0,
            (int)slider.Maximum);
        syncing = true;
        slider.Value = sliderValue;
        number.Value = toDecimal(clamped);
        syncing = false;
        if (!emit)
            return;
        JsonNode? result = returnInteger
            ? JsonValue.Create((int)Math.Round(clamped))
            : JsonValue.Create(roundToStep(clamped, range.Step));
        ValueChanged?.Invoke(this, new BlueprintProgressValueChangedEventArgs(result));
    }

    private static string createFormatString(double step)
    {
        string text = step.ToString("0.########", CultureInfo.InvariantCulture);
        int separator = text.IndexOf('.');
        if (separator < 0)
            return "0";
        int decimals = Math.Min(6, text.Length - separator - 1);
        return decimals <= 0 ? "0" : $"0.{new string('#', decimals)}";
    }

    private static double roundToStep(double value, double step)
    {
        string text = step.ToString("0.########", CultureInfo.InvariantCulture);
        int separator = text.IndexOf('.');
        int decimals = separator < 0 ? 0 : Math.Min(6, text.Length - separator - 1);
        return Math.Round(value, decimals);
    }

    private static double readDouble(JsonNode? value, double fallback)
    {
        if (value is JsonValue json)
        {
            if (json.TryGetValue(out double number))
                return number;
            if (json.TryGetValue(out int integer))
                return integer;
            if (json.TryGetValue(out decimal decimalValue))
                return decimal.ToDouble(decimalValue);
        }
        return double.TryParse(value?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out double parsed)
            ? parsed
            : fallback;
    }

    private static decimal toDecimal(double value)
    {
        if (value >= (double)decimal.MaxValue)
            return decimal.MaxValue;
        if (value <= (double)decimal.MinValue)
            return decimal.MinValue;
        return (decimal)value;
    }
}

internal sealed class BlueprintProgressValueChangedEventArgs(JsonNode? value) : EventArgs
{
    public JsonNode? Value { get; } = value;
}

internal sealed class BlueprintColourSwatch : Button
{
    private readonly Border fill = new()
    {
        Margin = new Thickness(1),
        HorizontalAlignment = HorizontalAlignment.Stretch,
        VerticalAlignment = VerticalAlignment.Stretch,
    };
    private Color colour;

    public BlueprintColourSwatch(Color initial)
    {
        Width = 54;
        Height = 28;
        MinWidth = 54;
        Padding = new Thickness(0);
        BorderBrush = new SolidColorBrush(Color.Parse("#464646"));
        BorderThickness = new Thickness(1);
        Grid content = new()
        {
            Width = 50,
            Height = 24,
        };
        content.Children.Add(new BlueprintCheckerboard
        {
            CellSize = 5,
            Margin = new Thickness(1),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
        });
        content.Children.Add(fill);
        Content = content;
        Colour = initial;
    }

    public Color Colour
    {
        get => colour;
        set
        {
            colour = value;
            fill.Background = new SolidColorBrush(value);
        }
    }
}

internal sealed class BlueprintCheckerboard : Control
{
    public static readonly StyledProperty<int> CellSizeProperty =
        AvaloniaProperty.Register<BlueprintCheckerboard, int>(nameof(CellSize), 5);

    public int CellSize
    {
        get => GetValue(CellSizeProperty);
        set => SetValue(CellSizeProperty, value);
    }

    public override void Render(DrawingContext context)
    {
        int size = Math.Max(1, CellSize);
        int columns = Math.Max(1, (int)Math.Ceiling(Bounds.Width / size));
        int rows = Math.Max(1, (int)Math.Ceiling(Bounds.Height / size));
        IBrush light = new SolidColorBrush(Color.Parse("#d8d8d8"));
        IBrush dark = new SolidColorBrush(Color.Parse("#9c9c9c"));
        for (int row = 0; row < rows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                context.FillRectangle(
                    (row + column) % 2 == 0 ? light : dark,
                    new Rect(column * size, row * size, size, size));
            }
        }
    }
}

internal sealed class BlueprintStructureWindow : Window
{
    private readonly BlueprintVariableForm variableForm;
    private readonly JsonObject value;

    private BlueprintStructureWindow(
        string title,
        IReadOnlyList<BlueprintVariableField> fields,
        string assetsDirectory,
        int cellSize,
        IGameVariableCatalog? gameVariables,
        bool readOnly)
    {
        Title = title;
        double contentHeight = Math.Max(200, fields.Count * 38 + 68);
        Width = 520;
        Height = Math.Min(contentHeight, 640);
        MinWidth = 420;
        MinHeight = Math.Min(contentHeight, 640);
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        value = [];
        foreach (BlueprintVariableField field in fields)
        {
            JsonNode? fieldValue = field.PreserveNullValue
                ? field.Value
                : field.Value ?? field.DefaultValue;
            value[field.Name] = fieldValue?.DeepClone();
        }
        variableForm = new BlueprintVariableForm
        {
            AssetsDirectory = assetsDirectory,
            CellSize = cellSize,
            GameVariables = gameVariables,
            IsReadOnly = readOnly,
        };
        variableForm.ValueChanged += (_, args) => value[args.Name] = args.Value?.DeepClone();
        variableForm.SetFields(fields);

        ScrollViewer scroll = new()
        {
            Content = variableForm,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        Button confirm = new()
        {
            Content = LocaleService.Get("CONFIRM"),
            IsEnabled = !readOnly,
        };
        confirm.Click += (_, _) => Close(value.DeepClone() as JsonObject);
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close(null);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        Grid layout = new()
        {
            Margin = new Thickness(10),
            RowDefinitions = new RowDefinitions("*,Auto"),
            RowSpacing = 8,
        };
        layout.Children.Add(scroll);
        Grid.SetRow(actions, 1);
        layout.Children.Add(actions);
        Content = layout;
        KeyDown += (_, args) =>
        {
            if (args.Key != Key.Escape)
                return;
            Close(null);
            args.Handled = true;
        };
    }

    public static System.Threading.Tasks.Task<JsonObject?> ShowAsync(
        Window owner,
        string title,
        IReadOnlyList<BlueprintVariableField> fields,
        string assetsDirectory,
        int cellSize,
        IGameVariableCatalog? gameVariables,
        bool readOnly)
    {
        BlueprintStructureWindow window = new(
            title,
            fields,
            assetsDirectory,
            cellSize,
            gameVariables,
            readOnly);
        return window.ShowDialog<JsonObject?>(owner);
    }
}
