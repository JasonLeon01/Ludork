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
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm : UserControl, IDisposable
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
    private ProjectDataStore? historyGameData;
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

    public ProjectDataStore? HistoryGameData
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
            values[field.Name] = (getFieldValue(field))?.DeepClone();
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
                values[entry.Key] = (entry.Value)?.DeepClone();
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
        contextValues[name] = (value)?.DeepClone();
        values[name] = (value)?.DeepClone();
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
        field.Value = (value)?.DeepClone();
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
            string description = field.Description;
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
        string description = field.Description;
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
            Control plainEditor = PlainTextEditorFactory(new BlueprintVariableEditorRequest(resolved, (displayValue)?.DeepClone(), changed));
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
            new BlueprintVariableEditorRequest(field, (displayValue)?.DeepClone(), changed));
        if (customEditor is not null)
            return customEditor;

        if (BillboardItemEditor.Supports(field))
        {
            return new BillboardItemEditor(
                new BlueprintVariableEditorRequest(field, displayValue, changed),
                AssetsDirectory,
                CellSize,
                GameVariables,
                isReadOnly || field.IsReadOnly);
        }

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

    private void attachHistory(TextBox control)
    {
        historyControls.Add(control);
        if (HistoryGameData is ProjectDataStore gameData)
            HistoryMergeBehavior.Attach(control, gameData);
    }

    private void attachHistory(NumericUpDown control)
    {
        historyControls.Add(control);
        if (HistoryGameData is ProjectDataStore gameData)
            HistoryMergeBehavior.Attach(control, gameData);
    }

    private void commit(BlueprintVariableField field, JsonNode? value, bool refresh)
    {
        if (disposed || building || isReadOnly || field.IsReadOnly)
            return;
        values.TryGetValue(field.Name, out JsonNode? previous);
        if (JsonNode.DeepEquals(previous, value))
            return;
        JsonNode? next = (value)?.DeepClone();
        field.Value = (next)?.DeepClone();
        if (next is null)
            field.PreserveNullValue = true;
        values[field.Name] = (next)?.DeepClone();
        refreshDependencyStates();
        if (building)
            return;
        bool requiresRefresh = refresh || dependencySources.Contains(field.Name);
        ValueChanged?.Invoke(
            this,
            new BlueprintVariableValueChangedEventArgs(field.Name, (next)?.DeepClone(), requiresRefresh));
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
            || !JsonScalar.TryGetString(value, out string name)
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
            field.Value = (value)?.DeepClone();
            field.PreserveNullValue = value is null;
            values[field.Name] = (value)?.DeepClone();
            if (!building)
            {
                ValueChanged?.Invoke(
                    this,
                    new BlueprintVariableValueChangedEventArgs(field.Name, (value)?.DeepClone(), true));
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
            if (JsonScalar.TryGetString(item, out string type) && !string.IsNullOrWhiteSpace(type))
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
}
