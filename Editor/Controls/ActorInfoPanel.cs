using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class ActorInfoPanel : UserControl
{
    private const double ClassContentMinimumWidth = 379;
    private readonly Border titleContainer;
    private readonly ScrollViewer scrollArea;
    private readonly TextBox tagEdit;
    private readonly TextBox blueprintPath;
    private readonly Button blueprintOpenButton;
    private readonly Button blueprintLocateButton;
    private readonly NumericUpDown positionX;
    private readonly NumericUpDown positionY;
    private readonly TextBlock layerReadOnlyLabel;
    private readonly Border classSeparator;
    private readonly TextBlock classTitleLabel;
    private readonly Grid classTitleContainer;
    private readonly Button resetAllButton;
    private readonly BlueprintVariableForm classForm;
    private readonly TextBlock noSelectionLabel;
    private readonly Dictionary<string, JsonNode?> defaultValues = new(StringComparer.Ordinal);
    private readonly Dictionary<string, JsonNode?> displayValues = new(StringComparer.Ordinal);
    private readonly HashSet<string> fieldsWithDefaults = new(StringComparer.Ordinal);
    private readonly HashSet<string> overriddenFields = new(StringComparer.Ordinal);
    private readonly Dictionary<string, Button> resetButtons = new(StringComparer.Ordinal);
    private GameDataService? gameData;
    private IMapEditingContext? editingContext;
    private string? runtimeActorId;
    private JsonObject? displayedRuntimeValues;
    private JsonObject? displayedRuntimeSchema;
    private bool isRuntime => editingContext?.IsRuntime == true;
    private bool canEditActor => layerEditable && editingContext?.IsEditable == true;
    private bool canMoveActor => canEditActor && (!isRuntime || getActorData()?["parentRuntimeId"] is null);
    private LuaMetadataService? metadataService;
    private BlueprintClassResolver? classResolver;
    private BlueprintVariableFieldBuilder? fieldBuilder;
    private MapPanel? editorPanel;
    private string? mapKey;
    private string? layerName;
    private int? actorIndex;
    private string? actorTag;
    private bool loading;
    private bool layerEditable = true;
    private string? blueprintReference;

    public event EventHandler<ActorSelectionChangedEventArgs>? ActorTagChanged;
    public event EventHandler<string>? BlueprintOpenRequested;
    public event EventHandler<string>? BlueprintLocateRequested;

    public ActorInfoPanel()
    {
        FontSize = 12;
        TextBlock titleLabel = new()
        {
            Text = LocaleService.Get("ACTOR_INFO"),
            FontWeight = FontWeight.Bold,
            HorizontalAlignment = HorizontalAlignment.Center,
            TextAlignment = TextAlignment.Center,
        };
        titleContainer = new Border
        {
            Height = 24,
            Background = EditorTheme.Brush("Input"),
            CornerRadius = new CornerRadius(4),
            Padding = new Thickness(4),
            Child = titleLabel,
        };

        tagEdit = EditorInputs.CreateEditableTextBox();
        tagEdit.HorizontalAlignment = HorizontalAlignment.Stretch;
        tagEdit.TextChanged += onTagChanged;
        Grid tagRow = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 4,
        };
        TextBlock tagLabel = new()
        {
            Text = LocaleService.Get("TAG"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        tagRow.Children.Add(tagLabel);
        Grid.SetColumn(tagEdit, 1);
        tagRow.Children.Add(tagEdit);

        blueprintPath = EditorInputs.CreateReadOnlyTextBox();
        blueprintPath.HorizontalAlignment = HorizontalAlignment.Stretch;
        blueprintOpenButton = new Button
        {
            Content = LocaleService.Get("OPEN"),
            Height = EditorInputs.FieldMinHeight,
            Padding = new Thickness(8, 0),
        };
        blueprintOpenButton.Click += (_, _) => requestBlueprintOpen();
        blueprintLocateButton = new Button
        {
            Content = LocaleService.Get("LOCATE"),
            Height = EditorInputs.FieldMinHeight,
            Padding = new Thickness(8, 0),
        };
        blueprintLocateButton.Click += (_, _) => requestBlueprintLocate();
        Grid blueprintRow = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto,Auto"),
            ColumnSpacing = 4,
        };
        TextBlock blueprintLabel = new()
        {
            Text = LocaleService.Get("BLUEPRINT"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        blueprintRow.Children.Add(blueprintLabel);
        Grid.SetColumn(blueprintPath, 1);
        blueprintRow.Children.Add(blueprintPath);
        Grid.SetColumn(blueprintOpenButton, 2);
        blueprintRow.Children.Add(blueprintOpenButton);
        Grid.SetColumn(blueprintLocateButton, 3);
        blueprintRow.Children.Add(blueprintLocateButton);

        positionX = EditorInputs.CreateNumericUpDown(0, 0, 0, 1);
        positionY = EditorInputs.CreateNumericUpDown(0, 0, 0, 1);
        positionX.ValueChanged += onPositionChanged;
        positionY.ValueChanged += onPositionChanged;
        Grid positionEditors = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*,Auto,*"),
            ColumnSpacing = 4,
        };
        TextBlock xLabel = new()
        {
            Text = "X",
            VerticalAlignment = VerticalAlignment.Center,
        };
        TextBlock yLabel = new()
        {
            Text = "Y",
            VerticalAlignment = VerticalAlignment.Center,
        };
        positionEditors.Children.Add(xLabel);
        Grid.SetColumn(positionX, 1);
        positionEditors.Children.Add(positionX);
        Grid.SetColumn(yLabel, 2);
        positionEditors.Children.Add(yLabel);
        Grid.SetColumn(positionY, 3);
        positionEditors.Children.Add(positionY);
        Grid positionRow = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 4,
        };
        TextBlock positionLabel = new()
        {
            Text = LocaleService.Get("POSITION"),
            VerticalAlignment = VerticalAlignment.Center,
        };
        positionRow.Children.Add(positionLabel);
        Grid.SetColumn(positionEditors, 1);
        positionRow.Children.Add(positionEditors);

        layerReadOnlyLabel = new TextBlock
        {
            Text = LocaleService.Get("LAYER_READ_ONLY"),
            Foreground = new SolidColorBrush(Color.Parse("#d9a441")),
            TextWrapping = TextWrapping.Wrap,
            IsVisible = false,
        };

        classSeparator = new Border
        {
            Height = 3,
            Background = new SolidColorBrush(Color.Parse("#464646")),
        };
        classTitleLabel = new TextBlock
        {
            Text = LocaleService.Get("CLASS_DETAIL"),
            FontWeight = FontWeight.Bold,
            VerticalAlignment = VerticalAlignment.Center,
        };
        resetAllButton = new Button
        {
            Content = LocaleService.Get("RESET_ALL_OVERRIDES"),
            Height = 28,
            Padding = new Thickness(8, 0),
            HorizontalAlignment = HorizontalAlignment.Right,
        };
        resetAllButton.Click += (_, _) => resetAllOverrides();
        classTitleContainer = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            Margin = new Thickness(0, 4, 0, 2),
        };
        classTitleContainer.Children.Add(classTitleLabel);
        Grid.SetColumn(resetAllButton, 1);
        classTitleContainer.Children.Add(resetAllButton);
        classForm = new BlueprintVariableForm { ShowSourceGroups = true };
        classForm.FieldActionFactory = createResetAction;
        classForm.ValueChanged += onClassVariableChanged;

        StackPanel content = new()
        {
            Width = ClassContentMinimumWidth,
            Spacing = 4,
            Children =
            {
                tagRow,
                blueprintRow,
                positionRow,
                layerReadOnlyLabel,
                classSeparator,
                classTitleContainer,
                classForm,
            },
        };
        scrollArea = new ScrollViewer
        {
            Content = content,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        scrollArea.ScrollChanged += (_, args) =>
        {
            if (args.ViewportDelta.X != 0)
                content.Width = Math.Max(ClassContentMinimumWidth, scrollArea.Viewport.Width);
        };

        noSelectionLabel = new TextBlock
        {
            Text = LocaleService.Get("GENERAL_DATA_PLACEHOLDER"),
            Foreground = Ludork.Services.EditorTheme.Brush("TextMuted"),
            FontStyle = FontStyle.Italic,
            HorizontalAlignment = HorizontalAlignment.Center,
            TextAlignment = TextAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 20, 0, 0),
        };

        Grid root = new()
        {
            Margin = new Thickness(4),
            RowDefinitions = new RowDefinitions("Auto,*"),
            RowSpacing = 4,
        };
        root.Children.Add(titleContainer);
        Grid.SetRow(scrollArea, 1);
        root.Children.Add(scrollArea);
        Grid.SetRow(noSelectionLabel, 1);
        root.Children.Add(noSelectionLabel);
        Content = root;
        Background = new SolidColorBrush(Color.Parse("#31363b"));
        showSelection(false);
    }

    public void configure(
        GameDataService nextGameData,
        LuaMetadataService nextMetadataService,
        BlueprintClassResolver nextClassResolver,
        IGameVariableCatalog nextGameVariables,
        MapPanel nextEditorPanel)
    {
        gameData = nextGameData;
        metadataService = nextMetadataService;
        classResolver = nextClassResolver;
        fieldBuilder = new BlueprintVariableFieldBuilder(nextGameData, nextMetadataService);
        editorPanel = nextEditorPanel;
        classForm.AssetsDirectory = Path.Combine(nextGameData.ProjectPath, "Assets");
        classForm.ProjectDirectory = nextGameData.ProjectPath;
        classForm.CellSize = nextGameData.getCellSize();
        classForm.GameVariables = nextGameVariables;
        classForm.HistoryGameData = nextGameData;
        HistoryMergeBehavior.Attach(tagEdit, nextGameData);
        HistoryMergeBehavior.Attach(positionX, nextGameData);
        HistoryMergeBehavior.Attach(positionY, nextGameData);
        HistoryMergeBehavior.AttachBoundary(this, nextGameData);
        ConfigureEditingContext(new ProjectMapEditingContext(nextGameData));
    }

    public void setActor(
        string nextMapKey,
        string? nextLayerName,
        int? nextActorIndex,
        JsonObject? actorData)
    {
        bool sameRuntimeActor = isRuntime && runtimeActorId is not null
            && string.Equals(runtimeActorId, actorData?["runtimeId"]?.GetValue<string>(), StringComparison.Ordinal)
            && string.Equals(mapKey, nextMapKey, StringComparison.Ordinal);
        mapKey = string.IsNullOrWhiteSpace(nextMapKey) ? null : nextMapKey;
        layerName = nextLayerName;
        actorIndex = nextActorIndex;
        runtimeActorId = isRuntime ? actorData?["runtimeId"]?.GetValue<string>() : null;
        if (sameRuntimeActor)
        {
            refreshRuntimeActorInfo();
            return;
        }
        displayedRuntimeValues = null;
        displayedRuntimeSchema = null;
        if (mapKey is null || layerName is null || actorIndex is null || actorData is null)
        {
            mapKey = null;
            layerName = null;
            actorIndex = null;
            actorTag = null;
            blueprintReference = null;
            runtimeActorId = null;
            showSelection(false);
            clearClassDetail();
            return;
        }

        showSelection(true);
        loading = true;
        actorTag = actorData["tag"]?.GetValue<string>() ?? string.Empty;
        tagEdit.Text = actorTag;
        blueprintReference = actorData["bp"]?.GetValue<string>();
        if (isRuntime && string.IsNullOrWhiteSpace(blueprintReference))
            blueprintReference = actorData["type"]?.GetValue<string>();
        blueprintPath.Text = blueprintReference ?? string.Empty;
        updatePositionEditors(actorData);
        loading = false;
        updateEditableState();
        refreshClassDetail();
    }

    public void setLayerEditable(bool editable)
    {
        if (layerEditable == editable)
            return;
        layerEditable = editable;
        updateEditableState();
    }

    internal void refreshActorProperties() => refreshClassDetail();

    public void refreshActorPosition()
    {
        if (isRuntime)
        {
            refreshRuntimeActorInfo();
            return;
        }
        JsonObject? actorData = getActorData();
        if (actorData is null)
            return;
        loading = true;
        updatePositionEditors(actorData);
        loading = false;
    }

    private void showSelection(bool selected)
    {
        IsEnabled = selected;
        noSelectionLabel.IsVisible = !selected;
        titleContainer.IsVisible = selected;
        scrollArea.IsVisible = selected;
    }

    private void clearClassDetail()
    {
        defaultValues.Clear();
        displayValues.Clear();
        fieldsWithDefaults.Clear();
        overriddenFields.Clear();
        resetButtons.Clear();
        classForm.Clear();
        setClassDetailVisible(false);
    }

    private void setClassDetailVisible(bool visible)
    {
        classSeparator.IsVisible = visible;
        classTitleContainer.IsVisible = visible;
        classForm.IsVisible = visible;
    }

    private void updateEditableState()
    {
        bool editable = canEditActor && mapKey is not null && layerName is not null && actorIndex is not null;
        if (editable && !isRuntime)
            EditorInputs.ApplyEditable(tagEdit);
        else
            EditorInputs.ApplyReadOnly(tagEdit);
        positionX.IsReadOnly = !editable || !canMoveActor;
        positionY.IsReadOnly = !editable || !canMoveActor;
        positionX.Focusable = editable && canMoveActor;
        positionY.Focusable = editable && canMoveActor;
        classForm.IsReadOnly = !editable;
        bool hasBlueprint = tryGetProjectBlueprintReference(out _);
        blueprintOpenButton.IsEnabled = !isRuntime && hasBlueprint;
        blueprintLocateButton.IsEnabled = hasBlueprint;
        layerReadOnlyLabel.IsVisible = mapKey is not null && !editable;
        resetAllButton.IsVisible = !isRuntime;
        resetAllButton.IsEnabled = editable && !isRuntime && overriddenFields.Count != 0;
        foreach (Button button in resetButtons.Values)
            button.IsEnabled = editable && !isRuntime;
    }

    private void updatePositionEditors(JsonObject actorData)
    {
        JsonObject? map = getMapData();
        int width = Math.Max(1, getInt(map?["width"], 1));
        int height = Math.Max(1, getInt(map?["height"], 1));
        positionX.Minimum = 0;
        positionX.Maximum = width - 1;
        positionY.Minimum = 0;
        positionY.Maximum = height - 1;
        JsonArray? position = actorData["position"] as JsonArray;
        positionX.Value = Math.Clamp(getInt(position?.ElementAtOrDefault(0), 0), 0, width - 1);
        positionY.Value = Math.Clamp(getInt(position?.ElementAtOrDefault(1), 0), 0, height - 1);
    }

    private void requestBlueprintOpen()
    {
        if (!isRuntime && tryGetProjectBlueprintReference(out string reference))
            BlueprintOpenRequested?.Invoke(this, reference);
    }

    private void requestBlueprintLocate()
    {
        if (tryGetProjectBlueprintReference(out string reference))
            BlueprintLocateRequested?.Invoke(this, reference);
    }

    private bool tryGetProjectBlueprintReference(out string reference)
    {
        const string prefix = "Data.Blueprints.";
        reference = blueprintReference ?? string.Empty;
        return reference.StartsWith(prefix, StringComparison.Ordinal)
            && reference.Length > prefix.Length
            && gameData?.BlueprintsData.ContainsKey(reference[prefix.Length..].Replace('.', '/')) == true;
    }

    private Control createResetAction(BlueprintVariableField field)
    {
        Button button = new()
        {
            Content = "↶",
            Width = 24,
            Height = 28,
            Padding = new Thickness(0),
            IsVisible = overriddenFields.Contains(field.Name),
            IsEnabled = layerEditable,
        };
        ToolTip.SetTip(button, LocaleService.Get("RESET_OVERRIDE"));
        button.Click += (_, _) => resetOverride(field.Name);
        resetButtons[field.Name] = button;
        return button;
    }

    private void updateResetActions()
    {
        foreach (KeyValuePair<string, Button> pair in resetButtons)
        {
            pair.Value.IsVisible = !isRuntime && overriddenFields.Contains(pair.Key);
            pair.Value.IsEnabled = canEditActor && !isRuntime;
        }
        resetAllButton.IsEnabled = canEditActor && !isRuntime && overriddenFields.Count != 0;
    }

    private void resetOverride(string name)
    {
        if (isRuntime || !canEditActor || gameData is null || editorPanel is null || getEditableActorData() is not JsonObject actorData
            || getClassVarChanges(actorData) is not JsonObject changes
            || !changes.ContainsKey(name))
        {
            return;
        }
        if (mapKey is null || layerName is null || actorIndex is not int index || actorTag is null
            || !gameData.RemoveMapActorOverrides(mapKey, layerName, index, actorTag, name))
        {
            refreshActorInfo();
            return;
        }
        editorPanel.refreshSelectedActor();
        refreshClassDetail();
    }

    private void resetAllOverrides()
    {
        if (isRuntime || !canEditActor || gameData is null || editorPanel is null || getEditableActorData() is not JsonObject actorData
            || getClassVarChanges(actorData) is not JsonObject changes
            || changes.Count == 0)
        {
            return;
        }
        if (mapKey is null || layerName is null || actorIndex is not int index || actorTag is null
            || !gameData.RemoveMapActorOverrides(mapKey, layerName, index, actorTag))
        {
            refreshActorInfo();
            return;
        }
        editorPanel.refreshSelectedActor();
        refreshClassDetail();
    }

    private void refreshClassDetail()
    {
        if (isRuntime)
        {
            refreshRuntimeClassDetail();
            return;
        }
        JsonObject? actorData = getActorData();
        string? reference = actorData?["bp"]?.GetValue<string>();
        if (actorData is null || string.IsNullOrWhiteSpace(reference)
            || classResolver is null || metadataService is null || fieldBuilder is null)
        {
            clearClassDetail();
            return;
        }

        JsonObject? overrides = getClassVarChanges(actorData);
        ResolvedBlueprintClass resolved = classResolver.Resolve(reference, overrides);
        bool knownClass = resolved.Fields.Count != 0
            || resolved.RootType is not null && metadataService.GetType(resolved.RootType) is not null;
        if (!knownClass)
        {
            clearClassDetail();
            return;
        }

        defaultValues.Clear();
        displayValues.Clear();
        fieldsWithDefaults.Clear();
        overriddenFields.Clear();
        resetButtons.Clear();
        if (overrides is not null)
        {
            foreach (string name in overrides.Select(pair => pair.Key))
                overriddenFields.Add(name);
        }
        List<BlueprintVariableField> formFields = [];
        foreach (BlueprintVariableField formField in fieldBuilder.Build(resolved))
        {
            if (string.Equals(formField.Name, "tag", StringComparison.Ordinal)
                || isBlueprintOnly(formField.Meta["BlueprintOnly"]))
                continue;
            formFields.Add(formField);
            displayValues[formField.Name] = cloneNode(formField.Value);
            if (resolved.GetField(formField.Name)?.HasBlueprintDefaultValue == true)
            {
                fieldsWithDefaults.Add(formField.Name);
                defaultValues[formField.Name] = cloneNode(formField.DefaultValue);
            }
        }

        classForm.SetFields(formFields);
        updateResetActions();
        setClassDetailVisible(true);
    }

    private static bool isBlueprintOnly(JsonNode? value)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out bool boolean)
            && boolean;
    }

    private void onTagChanged(object? sender, TextChangedEventArgs args)
    {
        if (isRuntime || loading || !canEditActor || gameData is null || editorPanel is null
            || mapKey is null || layerName is null || actorIndex is not int index || actorTag is null)
            return;
        string oldTag = actorTag;
        string? tag = gameData.RenameMapActorTag(mapKey, layerName, index, oldTag, tagEdit.Text ?? string.Empty);
        if (tag is null)
        {
            refreshActorInfo();
            return;
        }
        actorTag = tag;
        if (!string.Equals(tagEdit.Text, tag, StringComparison.Ordinal))
        {
            loading = true;
            tagEdit.Text = tag;
            loading = false;
        }
        if (string.Equals(oldTag, tag, StringComparison.Ordinal))
            return;
        editorPanel.refreshSelectedActor();
        ActorTagChanged?.Invoke(this, new ActorSelectionChangedEventArgs(
            mapKey,
            layerName,
            actorIndex,
            getActorData()));
    }

    private void onClassVariableChanged(
        object? sender,
        BlueprintVariableValueChangedEventArgs args)
    {
        if (isRuntime)
        {
            setRuntimeActorVariable(args);
            return;
        }
        if (loading || !canEditActor || gameData is null || editorPanel is null
            || mapKey is null || layerName is null || actorIndex is not int index || actorTag is null)
            return;
        JsonObject? actorData = getEditableActorData();
        if (actorData is null)
            return;
        JsonNode? value = cloneNode(args.Value);
        bool isDefault = fieldsWithDefaults.Contains(args.Name)
            && blueprintValuesEqual(value, defaultValues.GetValueOrDefault(args.Name));
        JsonObject? changes = getClassVarChanges(actorData);
        bool currentExists = changes?.ContainsKey(args.Name) == true;
        if (isDefault && !currentExists)
            return;
        if (!isDefault && currentExists
            && blueprintValuesEqual(changes![args.Name], value))
        {
            return;
        }

        bool updated = isDefault
            ? gameData.RemoveMapActorOverrides(mapKey, layerName, index, actorTag, args.Name)
            : editingContext?.SetActorVariable(mapKey, layerName, actorTag, args.Name, value) == true;
        if (!updated)
        {
            refreshActorInfo();
            return;
        }
        if (isDefault)
        {
            displayValues[args.Name] = cloneNode(defaultValues.GetValueOrDefault(args.Name));
            overriddenFields.Remove(args.Name);
        }
        else
        {
            displayValues[args.Name] = cloneNode(value);
            overriddenFields.Add(args.Name);
        }
        editorPanel.refreshSelectedActor();
        updateResetActions();
        if (args.RequiresRefresh)
            Dispatcher.UIThread.Post(refreshClassDetail);
    }

    private void onPositionChanged(object? sender, NumericUpDownValueChangedEventArgs args)
    {
        if (loading || !canMoveActor || editingContext is null || editorPanel is null
            || mapKey is null || layerName is null || actorIndex is not int index || actorTag is null)
            return;
        JsonObject? actorData = getEditableActorData();
        if (actorData is null)
            return;
        int x = decimal.ToInt32(positionX.Value ?? 0);
        int y = decimal.ToInt32(positionY.Value ?? 0);
        if (actorData["position"] is JsonArray position && position.Count >= 2
            && getInt(position[0], -1) == x && getInt(position[1], -1) == y)
        {
            return;
        }
        string actorId = isRuntime ? runtimeActorId ?? string.Empty : actorTag;
        if (actorId.Length == 0 || !editingContext.MoveActor(mapKey, layerName, actorId, x, y))
            refreshActorInfo();
        else
            editorPanel.refreshSelectedActor();
    }

    private JsonObject? getActorData()
    {
        if (gameData is null || mapKey is null || layerName is null || actorIndex is not int index)
            return null;
        JsonObject? map = getMapData();
        if (map?["actors"]?[layerName] is not JsonArray actors
            || index < 0 || index >= actors.Count)
        {
            return null;
        }
        return actors[index] as JsonObject;
    }

    private JsonObject? getMapData()
    {
        return editorPanel is not null && mapKey is not null
            && string.Equals(editorPanel.CurrentMapKey, mapKey, StringComparison.Ordinal)
            ? editorPanel.CurrentMapData
            : null;
    }

    private JsonObject? getEditableActorData()
    {
        JsonObject? actor = getActorData();
        string? identity = isRuntime ? runtimeActorId : actorTag;
        if (actor is not null && identity is not null
            && string.Equals(actor[isRuntime ? "runtimeId" : "tag"]?.GetValue<string>() ?? string.Empty, identity, StringComparison.Ordinal))
        {
            return actor;
        }
        refreshActorInfo();
        return null;
    }

    private JsonObject? getClassVarChanges(JsonObject actorData)
    {
        JsonObject? map = getMapData();
        if (map is null)
            return null;
        string tag = actorData[isRuntime ? "runtimeId" : "tag"]?.GetValue<string>() ?? string.Empty;
        return map["BPClassVarChanged"] is JsonObject root ? root[tag] as JsonObject : null;
    }

    private void refreshActorInfo()
    {
        setActor(mapKey ?? string.Empty, layerName, actorIndex, getActorData());
    }

    private static bool blueprintValuesEqual(JsonNode? left, JsonNode? right)
    {
        return JsonNode.DeepEquals(left, right);
    }

    private static int getInt(JsonNode? value, int fallback)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out int integer)
            ? integer
            : fallback;
    }

    private static JsonNode? cloneNode(JsonNode? value)
    {
        return value?.DeepClone();
    }
}
