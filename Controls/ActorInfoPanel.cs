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
    private readonly Border classSeparator;
    private readonly TextBlock classTitleLabel;
    private readonly BlueprintVariableForm classForm;
    private readonly TextBlock noSelectionLabel;
    private readonly Dictionary<string, JsonNode?> defaultValues = new(StringComparer.Ordinal);
    private readonly Dictionary<string, JsonNode?> displayValues = new(StringComparer.Ordinal);
    private readonly HashSet<string> fieldsWithDefaults = new(StringComparer.Ordinal);
    private GameDataService? gameData;
    private LuaMetadataService? metadataService;
    private BlueprintClassResolver? classResolver;
    private MapPanel? editorPanel;
    private string? mapKey;
    private string? layerName;
    private int? actorIndex;
    private bool loading;

    public event EventHandler<ActorSelectionChangedEventArgs>? ActorTagChanged;

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

        classSeparator = new Border
        {
            Height = 3,
            Background = new SolidColorBrush(Color.Parse("#464646")),
        };
        classTitleLabel = new TextBlock
        {
            Text = LocaleService.Get("CLASS_DETAIL"),
            FontWeight = FontWeight.Bold,
            Margin = new Thickness(0, 4, 0, 2),
        };
        classForm = new BlueprintVariableForm();
        classForm.ValueChanged += onClassVariableChanged;

        StackPanel content = new()
        {
            Width = ClassContentMinimumWidth,
            Spacing = 4,
            Children =
            {
                tagRow,
                classSeparator,
                classTitleLabel,
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
            Text = LocaleService.Get("NO_SELECTION"),
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
        MapPanel nextEditorPanel)
    {
        gameData = nextGameData;
        metadataService = nextMetadataService;
        classResolver = nextClassResolver;
        editorPanel = nextEditorPanel;
        classForm.AssetsDirectory = Path.Combine(nextGameData.ProjectPath, "Assets");
        classForm.ProjectDirectory = nextGameData.ProjectPath;
        classForm.CellSize = nextGameData.getCellSize();
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
            showSelection(false);
            clearClassDetail();
            return;
        }

        showSelection(true);
        loading = true;
        tagEdit.Text = actorData["tag"]?.GetValue<string>() ?? string.Empty;
        loading = false;
        refreshClassDetail();
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
        classForm.Clear();
        setClassDetailVisible(false);
    }

    private void setClassDetailVisible(bool visible)
    {
        classSeparator.IsVisible = visible;
        classTitleLabel.IsVisible = visible;
        classForm.IsVisible = visible;
    }

    private void refreshClassDetail()
    {
        JsonObject? actorData = getActorData();
        string? reference = actorData?["bp"]?.GetValue<string>();
        if (actorData is null || string.IsNullOrWhiteSpace(reference)
            || classResolver is null || metadataService is null)
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
        HashSet<string> invalidVars = new(resolved.InvalidVars, StringComparer.Ordinal)
        {
            "tag",
        };
        List<BlueprintVariableField> formFields = [];
        foreach (ResolvedBlueprintField field in resolved.Fields)
        {
            if (invalidVars.Contains(field.Name)
                || isBlueprintOnly(field.Metadata?.Meta["BlueprintOnly"])
                || field.IsUnknown && !field.HasBlueprintDefaultValue
                || !field.HasBlueprintDefaultValue && field.Metadata?.Component != true)
            {
                continue;
            }
            string? rectSource = getString(resolved.RectRangeVars[field.Name]);
            BlueprintVariableField formField = createFormField(
                field,
                field.BlueprintDefaultValue,
                field.HasBlueprintDefaultValue,
                rectSource,
                new HashSet<string>(StringComparer.Ordinal));
            formFields.Add(formField);
            displayValues[field.Name] = cloneNode(formField.Value);
            if (field.HasBlueprintDefaultValue)
            {
                fieldsWithDefaults.Add(field.Name);
                defaultValues[field.Name] = cloneNode(formField.DefaultValue);
            }
        }

        classForm.SetFields(formFields);
        setClassDetailVisible(true);
    }

    private static bool isBlueprintOnly(JsonNode? value)
    {
        return value is JsonValue scalar
            && scalar.TryGetValue(out bool boolean)
            && boolean;
    }

    private BlueprintVariableField createFormField(
        ResolvedBlueprintField field,
        JsonNode? defaultValue,
        bool hasDefault,
        string? rectSource,
        HashSet<string> resolving)
    {
        BlueprintFieldMetadata? fieldMetadata = field.Metadata;
        string? defaultModule = fieldMetadata?.DeclaringType.ModuleName;
        IReadOnlyList<BlueprintVariableField> nestedFields = createStructuredFields(
            field.Type,
            defaultModule,
            field.Value,
            resolving);
        LuaTypeReference displayType = field.Type.WithDefaultModule(defaultModule);
        JsonObject meta = fieldMetadata?.Meta.DeepClone() as JsonObject ?? [];
        return new BlueprintVariableField(field.Name, displayType.QualifiedName, field.Value)
        {
            Module = displayType.ModuleName,
            TypeName = displayType.TypeName,
            DefaultValue = hasDefault ? cloneNode(defaultValue) : null,
            DisplayValue = getConfigDisplayValue(field.Name, field.Value, meta),
            Meta = meta,
            IsComponent = fieldMetadata?.Component == true,
            PreserveNullValue = field.Value is null,
            RectSourceField = rectSource,
            Options = getGeneralDataOptions(meta),
            Fields = nestedFields,
        };
    }

    private IReadOnlyList<BlueprintVariableField> createStructuredFields(
        LuaTypeReference fieldType,
        string? defaultModule,
        JsonNode? value,
        HashSet<string> resolving)
    {
        if (metadataService is null)
            return [];
        LuaTypeReference type = fieldType.WithDefaultModule(defaultModule);
        if (metadataService.GetType(type) is null || !resolving.Add(type.QualifiedName))
            return [];

        IReadOnlyList<LuaTypeMetadata> mro = metadataService.ResolveMro(type);
        List<string> order = [];
        Dictionary<string, BlueprintFieldMetadata> schema = new(StringComparer.Ordinal);
        HashSet<string> invalidVars = new(StringComparer.Ordinal);
        Dictionary<string, string> rectSources = new(StringComparer.Ordinal);
        foreach (LuaTypeMetadata metadata in mro.Reverse())
        {
            foreach (string invalidVar in metadata.InvalidVars)
                invalidVars.Add(invalidVar);
            foreach (KeyValuePair<string, JsonNode?> pair in metadata.RectRangeVars)
            {
                string? source = getString(pair.Value);
                if (source is not null)
                    rectSources[pair.Key] = source;
            }
            foreach (string name in metadata.Attrs)
            {
                if (!metadata.Fields.TryGetValue(name, out BlueprintFieldMetadata? nestedMetadata))
                    continue;
                if (!schema.ContainsKey(name))
                    order.Add(name);
                schema[name] = nestedMetadata;
            }
        }

        JsonObject valueObject = value as JsonObject ?? [];
        List<BlueprintVariableField> result = [];
        HashSet<string> added = new(StringComparer.Ordinal);
        foreach (string name in order)
        {
            if (invalidVars.Contains(name))
                continue;
            BlueprintFieldMetadata metadata = schema[name];
            bool hasValue = valueObject.TryGetPropertyValue(name, out JsonNode? childValue);
            bool hasDefaultValue = metadata.HasDefaultValue;
            JsonNode? childDefault = metadata.DefaultValue;
            if (!hasValue && !hasDefaultValue)
                continue;
            if (!hasValue)
                childValue = childDefault;
            ResolvedBlueprintField nestedField = new(
                name,
                metadata.Type,
                childValue,
                childDefault,
                metadata,
                false,
                hasDefaultValue);
            rectSources.TryGetValue(name, out string? rectSource);
            result.Add(createFormField(
                nestedField,
                childDefault,
                hasDefaultValue,
                rectSource,
                resolving));
            added.Add(name);
        }

        foreach (KeyValuePair<string, JsonNode?> pair in valueObject)
        {
            if (added.Contains(pair.Key) || invalidVars.Contains(pair.Key))
                continue;
            ResolvedBlueprintField nestedField = new(
                pair.Key,
                inferType(pair.Value),
                pair.Value,
                null,
                null,
                true,
                false);
            result.Add(createFormField(
                nestedField,
                null,
                false,
                null,
                resolving));
        }
        resolving.Remove(type.QualifiedName);
        return result;
    }

    private void onTagChanged(object? sender, TextChangedEventArgs args)
    {
        if (loading || gameData is null || editorPanel is null)
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
        gameData.RecordSnapshot();
        actorData["tag"] = tag;
        moveClassVarChanges(oldTag, tag);
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
        if (loading || gameData is null || editorPanel is null)
            return;
        JsonObject? actorData = getActorData();
        if (actorData is null)
            return;
        JsonNode? value = cloneNode(args.Value);
        bool isDefault = fieldsWithDefaults.Contains(args.Name)
            && blueprintValuesEqual(value, defaultValues.GetValueOrDefault(args.Name));
        JsonObject? changes = getClassVarChanges(actorData, !isDefault);
        if (changes is null)
            return;
        bool currentExists = changes.ContainsKey(args.Name);
        if (isDefault && !currentExists)
            return;
        if (!isDefault && currentExists
            && blueprintValuesEqual(displayValues.GetValueOrDefault(args.Name), value))
        {
            return;
        }

        gameData.RecordSnapshot();
        if (isDefault)
        {
            changes.Remove(args.Name);
            cleanupClassVarChanges(actorData);
            displayValues[args.Name] = cloneNode(defaultValues.GetValueOrDefault(args.Name));
        }
        else
        {
            changes[args.Name] = cloneNode(value);
            displayValues[args.Name] = cloneNode(value);
        }
        gameData.refreshModifiedState();
        editorPanel.refreshSelectedActor();
        if (args.RequiresRefresh)
            Dispatcher.UIThread.Post(refreshClassDetail);
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

    private static LuaTypeReference inferType(JsonNode? value)
    {
        if (value is JsonObject)
            return new LuaTypeReference(null, "table");
        if (value is JsonArray)
            return new LuaTypeReference(null, "any[]");
        if (value is JsonValue scalar)
        {
            if (scalar.TryGetValue(out bool _))
                return new LuaTypeReference(null, "bool");
            if (scalar.TryGetValue(out string? _))
                return new LuaTypeReference(null, "string");
            if (scalar.TryGetValue(out int _) || scalar.TryGetValue(out long _))
                return new LuaTypeReference(null, "int");
            if (scalar.TryGetValue(out double _) || scalar.TryGetValue(out decimal _))
                return new LuaTypeReference(null, "float");
        }
        return new LuaTypeReference(null, "any");
    }

    private IReadOnlyList<BlueprintVariableOption> getGeneralDataOptions(JsonObject meta)
    {
        string? dataType = getGeneralDataType(meta["GeneralDataVars"]);
        if (dataType is null || gameData is null)
            return [];
        List<BlueprintVariableOption> options =
        [
            new BlueprintVariableOption(LocaleService.Get("GENERAL_DATA_PLACEHOLDER"), JsonValue.Create(string.Empty)),
        ];
        IEnumerable<string> keys;
        if (string.Equals(dataType, "ANIMATION", StringComparison.OrdinalIgnoreCase))
        {
            keys = gameData.AnimationsData.Keys;
        }
        else if (gameData.GeneralData.TryGetValue(dataType, out JsonObject? data)
            && data["members"] is JsonObject members)
        {
            keys = members.Select(pair => pair.Key);
        }
        else
        {
            keys = [];
        }
        foreach (string key in keys)
            options.Add(new BlueprintVariableOption(key, JsonValue.Create(key)));
        return options;
    }

    private JsonNode? getConfigDisplayValue(string fieldName, JsonNode? value, JsonObject meta)
    {
        if (gameData is null || getString(value) is not string text || text.Length != 0)
            return cloneNode(value);
        (string Config, string Setting)? reference = getConfigReference(meta["ConfigVars"], fieldName);
        if (reference is not { } configReference
            || !gameData.SystemConfigData.TryGetValue(configReference.Config, out JsonObject? config)
            || config[configReference.Setting] is not JsonObject setting
            || !setting.TryGetPropertyValue("value", out JsonNode? configValue))
        {
            return cloneNode(value);
        }
        return cloneNode(configValue);
    }

    private static string? getGeneralDataType(JsonNode? value)
    {
        if (getString(value) is string direct)
            return direct;
        if (value is JsonArray array)
        {
            if (array.Count == 1)
                return getString(array[0]);
            if (array.Count >= 2)
                return getString(array[1]) ?? getString(array[0]);
        }
        if (value is JsonObject data)
            return getString(data["type"] ?? data["dataType"] ?? data["key"]);
        return null;
    }

    private static (string Config, string Setting)? getConfigReference(JsonNode? value, string fieldName)
    {
        if (getString(value) is string direct)
        {
            int separator = direct.IndexOf('.');
            return separator > 0 && separator < direct.Length - 1
                ? (direct[..separator], direct[(separator + 1)..])
                : (direct, fieldName);
        }
        if (value is JsonArray array && array.Count >= 2
            && getString(array[^2]) is string config
            && getString(array[^1]) is string setting)
        {
            return (config, setting);
        }
        if (value is JsonObject reference
            && getString(reference["config"] ?? reference["file"]) is string configName
            && getString(reference["setting"] ?? reference["key"] ?? reference["name"]) is string settingName)
        {
            return (configName, settingName);
        }
        return null;
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

    private static string? getString(JsonNode? value)
    {
        return value is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }

    private static JsonNode? cloneNode(JsonNode? value)
    {
        return value?.DeepClone();
    }
}
