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

public sealed class ActorInfoPanel : UserControl
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
    private LuaMetadataService? metadataService;
    private BlueprintClassResolver? classResolver;
    private BlueprintVariableFieldBuilder? fieldBuilder;
    private MapPanel? editorPanel;
    private string? mapKey;
    private string? layerName;
    private int? actorIndex;
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
            Background = new SolidColorBrush(Color.Parse("#444444")),
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
            Height = 34,
            Padding = new Thickness(8, 0),
        };
        blueprintOpenButton.Click += (_, _) => requestBlueprintOpen();
        blueprintLocateButton = new Button
        {
            Content = LocaleService.Get("LOCATE"),
            Height = 34,
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
        classForm = new BlueprintVariableForm();
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
        scrollArea.SizeChanged += (_, args) =>
            content.Width = Math.Max(ClassContentMinimumWidth, args.NewSize.Width);

        noSelectionLabel = new TextBlock
        {
            Text = LocaleService.Get("GENERAL_DATA_PLACEHOLDER"),
            Foreground = new SolidColorBrush(Color.Parse("#888888")),
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
    }

    public void setActor(
        string nextMapKey,
        string? nextLayerName,
        int? nextActorIndex,
        JsonObject? actorData)
    {
        mapKey = string.IsNullOrWhiteSpace(nextMapKey) ? null : nextMapKey;
        layerName = nextLayerName;
        actorIndex = nextActorIndex;
        if (mapKey is null || layerName is null || actorIndex is null || actorData is null)
        {
            mapKey = null;
            layerName = null;
            actorIndex = null;
            blueprintReference = null;
            showSelection(false);
            clearClassDetail();
            return;
        }

        showSelection(true);
        loading = true;
        tagEdit.Text = actorData["tag"]?.GetValue<string>() ?? string.Empty;
        blueprintReference = actorData["bp"]?.GetValue<string>();
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

    public void refreshActorPosition()
    {
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
        bool editable = layerEditable && mapKey is not null && layerName is not null && actorIndex is not null;
        if (editable)
            EditorInputs.ApplyEditable(tagEdit);
        else
            EditorInputs.ApplyReadOnly(tagEdit);
        positionX.IsReadOnly = !editable;
        positionY.IsReadOnly = !editable;
        positionX.Focusable = editable;
        positionY.Focusable = editable;
        classForm.IsReadOnly = !editable;
        layerReadOnlyLabel.IsVisible = mapKey is not null && !editable;
        resetAllButton.IsEnabled = editable && overriddenFields.Count != 0;
        foreach (Button button in resetButtons.Values)
            button.IsEnabled = editable;
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
        if (!string.IsNullOrWhiteSpace(blueprintReference))
            BlueprintOpenRequested?.Invoke(this, blueprintReference);
    }

    private void requestBlueprintLocate()
    {
        if (!string.IsNullOrWhiteSpace(blueprintReference))
            BlueprintLocateRequested?.Invoke(this, blueprintReference);
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
            pair.Value.IsVisible = overriddenFields.Contains(pair.Key);
            pair.Value.IsEnabled = layerEditable;
        }
        resetAllButton.IsEnabled = layerEditable && overriddenFields.Count != 0;
    }

    private void resetOverride(string name)
    {
        if (!layerEditable || gameData is null || editorPanel is null || getActorData() is not JsonObject actorData
            || getClassVarChanges(actorData, false) is not JsonObject changes
            || !changes.ContainsKey(name))
        {
            return;
        }
        recordMapSnapshot();
        changes.Remove(name);
        cleanupClassVarChanges(actorData);
        markMapModified();
        editorPanel.refreshSelectedActor();
        refreshClassDetail();
    }

    private void resetAllOverrides()
    {
        if (!layerEditable || gameData is null || editorPanel is null || getActorData() is not JsonObject actorData
            || getClassVarChanges(actorData, false) is not JsonObject changes
            || changes.Count == 0)
        {
            return;
        }
        recordMapSnapshot();
        changes.Clear();
        cleanupClassVarChanges(actorData);
        markMapModified();
        editorPanel.refreshSelectedActor();
        refreshClassDetail();
    }

    private void refreshClassDetail()
    {
        JsonObject? actorData = getActorData();
        string? reference = actorData?["bp"]?.GetValue<string>();
        if (actorData is null || string.IsNullOrWhiteSpace(reference)
            || classResolver is null || metadataService is null || fieldBuilder is null)
        {
            clearClassDetail();
            return;
        }

        JsonObject? overrides = getClassVarChanges(actorData, false);
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
        if (loading || !layerEditable || gameData is null || editorPanel is null)
            return;
        JsonObject? actorData = getActorData();
        if (actorData is null)
            return;
        string oldTag = actorData["tag"]?.GetValue<string>() ?? string.Empty;
        string tag = editorPanel.makeUniqueActorTag(tagEdit.Text ?? string.Empty, layerName, actorIndex);
        if (!string.Equals(tagEdit.Text, tag, StringComparison.Ordinal))
        {
            loading = true;
            tagEdit.Text = tag;
            loading = false;
        }
        if (string.Equals(oldTag, tag, StringComparison.Ordinal))
            return;
        recordMapSnapshot();
        actorData["tag"] = tag;
        moveClassVarChanges(oldTag, tag);
        if (mapKey is not null)
            gameData.NotifyMapActorsChanged(mapKey);
        if (mapKey is not null)
            gameData.NotifyMapContentChanged(mapKey);
        gameData.refreshModifiedState();
        editorPanel.refreshSelectedActor();
        ActorTagChanged?.Invoke(this, new ActorSelectionChangedEventArgs(
            mapKey ?? string.Empty,
            layerName,
            actorIndex,
            actorData));
    }

    private void onClassVariableChanged(
        object? sender,
        BlueprintVariableValueChangedEventArgs args)
    {
        if (loading || !layerEditable || gameData is null || editorPanel is null)
            return;
        JsonObject? actorData = getActorData();
        if (actorData is null)
            return;
        JsonNode? value = cloneNode(args.Value);
        bool isDefault = fieldsWithDefaults.Contains(args.Name)
            && blueprintValuesEqual(value, defaultValues.GetValueOrDefault(args.Name));
        JsonObject? changes = getClassVarChanges(actorData, false);
        bool currentExists = changes?.ContainsKey(args.Name) == true;
        if (isDefault && !currentExists)
            return;
        if (!isDefault && currentExists
            && blueprintValuesEqual(displayValues.GetValueOrDefault(args.Name), value))
        {
            return;
        }

        recordMapSnapshot();
        if (isDefault)
        {
            changes!.Remove(args.Name);
            cleanupClassVarChanges(actorData);
            displayValues[args.Name] = cloneNode(defaultValues.GetValueOrDefault(args.Name));
            overriddenFields.Remove(args.Name);
        }
        else
        {
            changes ??= getClassVarChanges(actorData, true)!;
            changes[args.Name] = cloneNode(value);
            displayValues[args.Name] = cloneNode(value);
            overriddenFields.Add(args.Name);
        }
        markMapModified();
        editorPanel.refreshSelectedActor();
        updateResetActions();
        if (args.RequiresRefresh)
            Dispatcher.UIThread.Post(refreshClassDetail);
    }

    private void recordMapSnapshot()
    {
        if (gameData is null)
            return;
        if (mapKey is null)
            gameData.RecordSnapshot();
        else
            gameData.RecordMapSnapshot(mapKey);
    }

    private void markMapModified()
    {
        if (gameData is null)
            return;
        if (mapKey is not null)
            gameData.NotifyMapContentChanged(mapKey);
        gameData.refreshModifiedState();
    }

    private void onPositionChanged(object? sender, NumericUpDownValueChangedEventArgs args)
    {
        if (loading || !layerEditable || editorPanel is null)
            return;
        int x = decimal.ToInt32(positionX.Value ?? 0);
        int y = decimal.ToInt32(positionY.Value ?? 0);
        editorPanel.updateSelectedActorPosition(x, y);
    }

    private JsonObject? getActorData()
    {
        if (gameData is null || mapKey is null || layerName is null || actorIndex is not int index)
            return null;
        JsonObject? map = gameData.getMap(mapKey);
        if (map?["actors"]?[layerName] is not JsonArray actors
            || index < 0 || index >= actors.Count)
        {
            return null;
        }
        return actors[index] as JsonObject;
    }

    private JsonObject? getMapData()
    {
        return gameData is not null && mapKey is not null ? gameData.getMap(mapKey) : null;
    }

    private JsonObject? getClassVarChanges(JsonObject actorData, bool create)
    {
        JsonObject? map = getMapData();
        if (map is null)
            return null;
        string tag = actorData["tag"]?.GetValue<string>() ?? string.Empty;
        if (map["BPClassVarChanged"] is not JsonObject root)
        {
            if (!create)
                return null;
            root = [];
            map["BPClassVarChanged"] = root;
        }
        if (root[tag] is JsonObject changes)
            return changes;
        if (!create)
            return null;
        changes = [];
        root[tag] = changes;
        return changes;
    }

    private void cleanupClassVarChanges(JsonObject actorData)
    {
        JsonObject? map = getMapData();
        if (map?["BPClassVarChanged"] is not JsonObject root)
            return;
        string tag = actorData["tag"]?.GetValue<string>() ?? string.Empty;
        if (root[tag] is JsonObject changes && changes.Count == 0)
            root.Remove(tag);
        if (root.Count == 0)
            map.Remove("BPClassVarChanged");
    }

    private void moveClassVarChanges(string oldTag, string newTag)
    {
        if (string.Equals(oldTag, newTag, StringComparison.Ordinal))
            return;
        JsonObject? map = getMapData();
        if (map?["BPClassVarChanged"] is not JsonObject root
            || root[oldTag] is not JsonObject oldChanges)
        {
            return;
        }
        root.Remove(oldTag);
        if (root[newTag] is JsonObject newChanges)
        {
            foreach (KeyValuePair<string, JsonNode?> pair in oldChanges)
                newChanges[pair.Key] = cloneNode(pair.Value);
        }
        else
        {
            root[newTag] = oldChanges;
        }
        if (root.Count == 0)
            map.Remove("BPClassVarChanged");
    }

    private static bool blueprintValuesEqual(JsonNode? left, JsonNode? right)
    {
        if (ReferenceEquals(left, right))
            return true;
        if (left is null || right is null)
            return false;
        if (tryGetNumber(left, out double leftNumber)
            && tryGetNumber(right, out double rightNumber))
        {
            return Math.Abs(leftNumber - rightNumber) <= 0.0001;
        }
        if (left is JsonArray leftArray && right is JsonArray rightArray)
        {
            if (leftArray.Count != rightArray.Count)
                return false;
            for (int index = 0; index < leftArray.Count; index += 1)
            {
                if (!blueprintValuesEqual(leftArray[index], rightArray[index]))
                    return false;
            }
            return true;
        }
        if (left is JsonObject leftObject && right is JsonObject rightObject)
        {
            if (leftObject.Count != rightObject.Count)
                return false;
            foreach (KeyValuePair<string, JsonNode?> pair in leftObject)
            {
                if (!rightObject.TryGetPropertyValue(pair.Key, out JsonNode? rightValue)
                    || !blueprintValuesEqual(pair.Value, rightValue))
                {
                    return false;
                }
            }
            return true;
        }
        return JsonNode.DeepEquals(left, right);
    }

    private static bool tryGetNumber(JsonNode value, out double number)
    {
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out int integer))
            {
                number = integer;
                return true;
            }
            if (scalar.TryGetValue(out long longValue))
            {
                number = longValue;
                return true;
            }
            if (scalar.TryGetValue(out float floatValue))
            {
                number = floatValue;
                return true;
            }
            if (scalar.TryGetValue(out double doubleValue))
            {
                number = doubleValue;
                return true;
            }
            if (scalar.TryGetValue(out decimal decimalValue))
            {
                number = decimal.ToDouble(decimalValue);
                return true;
            }
        }
        number = 0;
        return false;
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
