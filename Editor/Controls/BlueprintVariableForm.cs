using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
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
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed class BlueprintVariableForm : UserControl, IDisposable
{
    private const double CompactDictionaryEntryWidth = 300;
    private readonly Grid form = createFormGrid();
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
    private int cellSize = EngineConstants.CellSize;
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
    public Func<BlueprintVariableEditorRequest, Control>? PlainTextEditorFactory { get; set; }
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
            foreach (Control control in historyControls)
            {
                if (historyGameData is null)
                    HistoryMergeBehavior.Detach(control);
                else if (control is TextBox text)
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

    public bool ShowSourceGroups { get; set; }

    public void SetFields(IEnumerable<BlueprintVariableField> nextFields)
    {
        if (disposed)
            return;
        editorRefreshGeneration++;
        prepareFields(nextFields);
        foreach (bool row in buildFieldRows())
        {
        }
        building = false;
        form.IsEnabled = true;
        refreshDependencyStates();
    }

    public async Task SetFieldsAsync(IEnumerable<BlueprintVariableField> nextFields, CancellationToken cancellationToken)
    {
        if (disposed)
            return;
        int generation = ++editorRefreshGeneration;
        prepareFields(nextFields);
        EditorUiBatch batch = new();
        try
        {
            foreach (bool row in buildFieldRows())
            {
                await batch.YieldIfNeededAsync(cancellationToken);
                if (disposed || generation != editorRefreshGeneration)
                    return;
            }
        }
        finally
        {
            if (generation == editorRefreshGeneration)
            {
                building = false;
                form.IsEnabled = true;
                refreshDependencyStates();
            }
        }
    }

    private void prepareFields(IEnumerable<BlueprintVariableField> nextFields)
    {
        building = true;
        form.IsEnabled = false;
        disposeNestedForms();
        fields.Clear();
        fields.AddRange(nextFields.Select(field => field.Clone()));
        values.Clear();
        rows.Clear();
        dependencySources.Clear();
        instanceVariableSources.Clear();
        foreach (Control control in historyControls)
            HistoryMergeBehavior.Detach(control);
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

    }

    private IEnumerable<bool> buildFieldRows()
    {
        List<BlueprintVariableField> componentFields = fields
            .Where(field => field.IsComponent)
            .ToList();
        if (componentFields.Count > 0)
        {
            addComponentRow(componentFields);
            yield return true;
        }

        IEnumerable<BlueprintVariableField> variableFields = fields.Where(field => !field.IsComponent);
        if (ShowSourceGroups)
        {
            foreach (IGrouping<string?, BlueprintVariableField> group in variableFields.GroupBy(
                field => field.SourceClass, StringComparer.Ordinal))
            {
                Grid target = string.IsNullOrWhiteSpace(group.Key) ? form : addSourceGroup(group.Key);
                foreach (BlueprintVariableField field in group)
                {
                    addFieldRow(field, target);
                    yield return true;
                }
            }
        }
        else
        {
            foreach (BlueprintVariableField field in variableFields)
            {
                addFieldRow(field, form);
                yield return true;
            }
        }

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
                getFieldValue(field) as JsonObject,
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
            Height = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
            IsEnabled = !isReadOnly && ComponentAddRequested is not null && addableFields.Count > 0,
        };
        add.Click += (_, _) => ComponentAddRequested?.Invoke(
            this,
            new BlueprintComponentFieldsEventArgs(addableFields));
        Button remove = new()
        {
            Content = "-",
            Width = 24,
            Height = EditorInputs.FieldMinHeight,
            Padding = new Thickness(0),
        };
        void updateRemoveState()
        {
            remove.IsEnabled = !isReadOnly
                && ComponentRemoveRequested is not null
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

    private static Grid createFormGrid()
    {
        return new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto"),
            ColumnSpacing = 4,
            RowSpacing = 4,
        };
    }

    private Grid addSourceGroup(string sourceClass)
    {
        TextBlock title = new()
        {
            Text = LuaTypeReference.Parse(sourceClass).TypeName,
            FontSize = 12,
            Foreground = new SolidColorBrush(Color.Parse("#909090")),
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        ToolTip.SetTip(title, sourceClass);
        Grid groupForm = createFormGrid();
        Grid content = new()
        {
            RowDefinitions = new RowDefinitions("Auto,Auto"),
            RowSpacing = 6,
            Margin = new Thickness(8),
            Children = { title, groupForm },
        };
        Grid.SetRow(groupForm, 1);
        Grid container = new()
        {
            Margin = new Thickness(0, form.RowDefinitions.Count == 0 ? 0 : 4, 0, 0),
            Children =
            {
                new Avalonia.Controls.Shapes.Rectangle
                {
                    Stroke = EditorTheme.Brush("Border"),
                    StrokeThickness = 1,
                    StrokeDashArray = [4, 3],
                    IsHitTestVisible = false,
                },
                content,
            },
        };
        int row = form.RowDefinitions.Count;
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid.SetRow(container, row);
        Grid.SetColumnSpan(container, 3);
        form.Children.Add(container);
        return groupForm;
    }

    private void addFieldRow(BlueprintVariableField field, Grid target)
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
            if (PlainTextEditorFactory is null)
                ToolTip.SetTip(editor, tooltip);
        }
        rows[field.Name] = new BlueprintVariableRow(field, label, editor, description, dependency);
        addGridRow(label, editor, FieldActionFactory?.Invoke(field), target);
    }

    private void addGridRow(Control label, Control editor, Control? action = null, Grid? target = null)
    {
        target ??= form;
        int row = target.RowDefinitions.Count;
        target.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid.SetRow(editor, row);
        if (showFieldNames)
        {
            label.VerticalAlignment = VerticalAlignment.Center;
            Grid.SetRow(label, row);
            target.Children.Add(label);
            Grid.SetColumn(editor, 1);
        }
        else
        {
            Grid.SetColumn(editor, 0);
            Grid.SetColumnSpan(editor, action is null ? 3 : 2);
        }
        target.Children.Add(editor);
        if (action is not null)
        {
            Grid.SetRow(action, row);
            Grid.SetColumn(action, 2);
            target.Children.Add(action);
        }
    }

    private Control createValueEditor(
        BlueprintVariableField field,
        JsonNode? displayValue,
        Action<JsonNode?, bool> changed,
        string? dictionaryKey = null,
        IReadOnlySet<string>? excludedInstanceVariableNames = null)
    {
        if (PlainTextEditorFactory is not null)
        {
            BlueprintVariableField resolved = resolveInstanceVariableValueField(field, dictionaryKey);
            Control plainEditor = PlainTextEditorFactory(new BlueprintVariableEditorRequest(resolved, cloneNode(displayValue), changed));
            if (plainEditor is TextBox text)
                attachHistory(text);
            return plainEditor;
        }
        if (getMetadataNode(field, "InstVar") is not null)
        {
            return createInstanceVariableEditor(
                field,
                displayValue,
                changed,
                excludedInstanceVariableNames);
        }

        field = resolveInstanceVariableValueField(field, dictionaryKey);
        if (field.EditorKind == BlueprintVariableEditorKind.ObjectReference)
        {
            TextBox input = createTextEditor(displayValue, true, changed);
            input.PlaceholderText = "self";
            return input;
        }
        LuaMetadataType declaredType = LuaMetadataType.Parse(field.Type);
        if (declaredType.Kind == LuaMetadataTypeKind.Union)
            return createUnionEditor(field, declaredType, displayValue, changed, dictionaryKey);
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

        if (valueType.Kind == LuaMetadataTypeKind.Named && valueType.Name == "nil")
            return EditorInputs.CreateReadOnlyTextBox("null");

        if (valueType.IsAny)
            return createAnyEditor(displayValue, changed);

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

    private Control createUnionEditor(
        BlueprintVariableField field,
        LuaMetadataType type,
        JsonNode? value,
        Action<JsonNode?, bool> changed,
        string? dictionaryKey)
    {
        ComboBox selector = new()
        {
            ItemsSource = type.Arguments.Select(branch => branch.ToString()).ToArray(),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            PlaceholderText = LocaleService.Get(value is null ? "UNION_SELECT_TYPE" : "UNION_INVALID_VALUE"),
        };
        StackPanel container = new() { Spacing = 4 };
        ContentControl content = new();
        container.Children.Add(selector);
        container.Children.Add(content);
        JsonObject? wrapper = value as JsonObject;
        int selectedIndex = wrapper is not null && wrapper.Count == 2
            && wrapper.ContainsKey("$value")
            ? type.Arguments.ToList().FindIndex(branch => JsonNode.DeepEquals(branch.ToSchema(), wrapper["$type"]))
            : -1;
        void showBranch(LuaMetadataType branch, JsonNode? branchValue)
        {
            if (!LuaMetadataValueDefaults.TryCreateLiteral(branch, out JsonNode? _))
            {
                TextBox unavailable = EditorInputs.CreateReadOnlyTextBox();
                unavailable.Text = branch.Name is "function" or "event"
                    ? LocaleService.Get("UNION_CONNECT_FUNCTION")
                    : LocaleService.Get("UNION_CONNECT_VALUE");
                content.Content = unavailable;
                return;
            }
            if (branch.Kind == LuaMetadataTypeKind.Named && branch.Name == "nil")
            {
                TextBox nil = EditorInputs.CreateReadOnlyTextBox();
                nil.Text = "null";
                content.Content = nil;
                return;
            }
            BlueprintVariableField branchField = createContainerItemField(field, branch, branchValue, field.Meta);
            content.Content = createValueEditor(
                branchField,
                branchValue,
                (next, refresh) => changed(LuaMetadataValueDefaults.WrapUnion(branch, next), refresh),
                dictionaryKey);
        }
        selector.SelectedIndex = selectedIndex;
        if (selectedIndex >= 0)
            showBranch(type.Arguments[selectedIndex], wrapper!["$value"]);
        selector.SelectionChanged += (_, _) =>
        {
            if (selector.SelectedIndex < 0 || selector.SelectedIndex == selectedIndex)
                return;
            selectedIndex = selector.SelectedIndex;
            LuaMetadataType branch = type.Arguments[selectedIndex];
            bool hasLiteral = LuaMetadataValueDefaults.TryCreateLiteral(branch, out JsonNode? next);
            showBranch(branch, next);
            changed(hasLiteral ? LuaMetadataValueDefaults.WrapUnion(branch, next) : null, false);
        };
        return container;
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
                value = cloneNode(option.Value);
                changed(cloneNode(value), false);
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
                source[componentIndex] = cloneNode(next);
                changed(cloneNode(source), refresh);
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
            string baseDirectory;
            if (projectRoot)
            {
                baseDirectory = getSafeDirectory(ProjectDirectory, assetSubdirectory);
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
                keyBox.PropertyChanged += (_, args) =>
                {
                    if (args.Property != TextBox.TextProperty)
                        return;
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
        JsonObject value = getFieldValue(field)?.DeepClone() as JsonObject ?? [];
        nested.ValueChanged += (_, args) =>
        {
            value[args.Name] = cloneNode(args.Value);
            changed(cloneNode(value), args.RequiresRefresh);
        };
        nested.SetFields(nestedFields);
        return new Border
        {
            BorderBrush = Ludork.Services.EditorTheme.Brush("Border"),
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
        if (disposed || building || isReadOnly || field.IsReadOnly)
            return;
        values.TryGetValue(field.Name, out JsonNode? previous);
        if (JsonNode.DeepEquals(previous, value))
            return;
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
                if (PlainTextEditorFactory is null)
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
        return JsonNode.DeepEquals(left, right);
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
        return value;
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
            return new VectorSpec(2, true, 0, uint.MaxValue);
        if (string.Equals(type, "sf.Vector3f", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3f", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, false, -999999999m, 999999999m);
        if (string.Equals(type, "sf.Vector3i", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3i", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, true, int.MinValue, int.MaxValue);
        if (string.Equals(type, "sf.Vector3u", StringComparison.OrdinalIgnoreCase)
            || string.Equals(type, "Vector3u", StringComparison.OrdinalIgnoreCase))
            return new VectorSpec(3, true, 0, uint.MaxValue);
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
        return field.Type;
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

    private static string formatNode(JsonNode? value)
    {
        if (value is null)
            return "null";
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
        if (decimal.TryParse(getText(value), NumberStyles.Float, CultureInfo.InvariantCulture, out decimal number))
            return number;
        double floating = getDouble(value);
        return floating > 0 ? decimal.MaxValue : floating < 0 ? decimal.MinValue : 0;
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
    ObjectReference,
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
