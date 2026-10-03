using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
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
            if (!branch.ContainsEnum && !LuaMetadataValueDefaults.TryCreateLiteral(branch, out JsonNode? _, readEnum))
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
            bool hasLiteral = LuaMetadataValueDefaults.TryCreateLiteral(branch, out JsonNode? next, readEnum);
            showBranch(branch, next);
            changed(hasLiteral ? LuaMetadataValueDefaults.WrapUnion(branch, next) : null, false);
        };
        return container;
    }

    private Control createSequenceEditor(
        BlueprintVariableField field,
        LuaMetadataType itemType,
        JsonArray? value,
        Action<JsonNode?, bool> changed,
        string? dictionaryKey)
    {
        JsonArray items = (value)?.DeepClone() as JsonArray ?? [];
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
                        items[itemIndex] = (next)?.DeepClone();
                        changed(items.DeepClone(), refresh);
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
                    changed(items.DeepClone(), true);
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
                changed(items.DeepClone(), true);
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
        JsonArray source = (value)?.DeepClone() as JsonArray ?? [];
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
                ? (source[index])?.DeepClone()
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
                    items[itemIndex] = (next)?.DeepClone();
                    changed(items.DeepClone(), refresh);
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
        JsonObject items = (value)?.DeepClone() as JsonObject ?? [];
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
            JsonNode? rowValue = (initialValue)?.DeepClone()
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
                        items[nextKey] = (rowValue)?.DeepClone();
                    changed(items.DeepClone(), false);
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
                        if (!JsonScalar.TryGetString(next, out string nextKey))
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
                        items[currentKey] = (rowValue)?.DeepClone();
                        if (isDraft)
                            hasDraft = false;
                        changed(items.DeepClone(), false);
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
                        rowValue = (next)?.DeepClone();
                        items[currentKey] = (rowValue)?.DeepClone();
                        changed(items.DeepClone(), refresh);
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
                    changed(items.DeepClone(), false);
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
                changed(items.DeepClone(), false);
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
            ProjectDirectory = ProjectDirectory,
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
            value[args.Name] = (args.Value)?.DeepClone();
            changed((value)?.DeepClone(), args.RequiresRefresh);
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
                    child.Value = (childValue)?.DeepClone();
                    if (!JsonNode.DeepEquals(definition.Value, childValue))
                        child.DisplayValue = null;
                    child.PreserveNullValue = childValue is null;
                }
                else if (child.Value is null)
                {
                    child.Value = (child.DefaultValue)?.DeepClone();
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
            result[field.Name] = (getFieldValue(field))?.DeepClone();
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
            return (options[0].Value)?.DeepClone();

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
            JsonArray source = (value)?.DeepClone() as JsonArray ?? [];
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
            JsonArray source = (value)?.DeepClone() as JsonArray ?? [];
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
        return (value)?.DeepClone();
    }

    private JsonNode? createDefaultNode(
        LuaMetadataType type,
        IReadOnlyList<BlueprintVariableField> structureFields)
    {
        if (structureFields.Count > 0)
            return buildStructureValue(structureFields);
        return LuaMetadataValueDefaults.Create(
            type,
            _ => JsonValue.Create(string.Empty),
            readEnum);
    }
}
