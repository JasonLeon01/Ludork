using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class BlueprintVariableForm
{
    private readonly List<MultipleFieldEditor> multipleFieldEditors = [];

    public void SetMultipleFieldStates(IReadOnlyList<BlueprintVariableField> nextFields)
    {
        foreach (MultipleFieldEditor item in multipleFieldEditors)
        {
            BlueprintVariableField? root = nextFields.FirstOrDefault(field => field.Name == item.Root.Name);
            BlueprintVariableField? next = root is null ? null : findMultipleField(root, item.Field.EditPath);
            item.Field.IsMultiEditReadOnly = next?.IsMultiEditReadOnly ?? true;
            item.Editor.IsEnabled = !isReadOnly && !item.Field.IsReadOnly && !item.Field.IsMultiEditReadOnly;
        }
    }

    private static BlueprintVariableField? findMultipleField(BlueprintVariableField root, IReadOnlyList<string> path)
    {
        if (root.EditPath.SequenceEqual(path))
            return root;
        foreach (BlueprintVariableField child in root.MultiEditChildren)
            if (findMultipleField(child, path) is BlueprintVariableField found)
                return found;
        return null;
    }

    private Control createMultipleValueEditor(BlueprintVariableField root, BlueprintVariableField field)
    {
        Control editor;
        if (field.IsMultiEditContainer)
        {
            Grid children = new()
            {
                ColumnDefinitions = new ColumnDefinitions("Auto,*"),
                ColumnSpacing = 4,
                RowSpacing = 4,
            };
            foreach (BlueprintVariableField child in field.MultiEditChildren)
            {
                int row = children.RowDefinitions.Count;
                children.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
                TextBlock label = new() { Text = getDisplayName(child), VerticalAlignment = VerticalAlignment.Center };
                Control childEditor = createMultipleValueEditor(root, child);
                Grid.SetRow(label, row);
                Grid.SetRow(childEditor, row);
                Grid.SetColumn(childEditor, 1);
                children.Children.Add(label);
                children.Children.Add(childEditor);
            }
            editor = field.MultiEditChildren.Count == 0
                ? EditorInputs.CreateReadOnlyTextBox(field.IsMixedValue ? LocaleService.Get("MULTIPLE_CHOICES") : getText(getFieldValue(field)))
                : new Border
                {
                    BorderBrush = EditorTheme.Brush("Border"),
                    BorderThickness = new Thickness(1),
                    Padding = new Thickness(6),
                    Child = children,
                };
        }
        else
        {
            void changed(JsonNode? value, bool refresh) => commitMultipleValue(root, field, value, refresh);
            if (getMetadataNode(field, "InstVar") is not null)
                editor = createMultipleInstanceVariableEditor(field, changed);
            else if (field.IsMixedValue)
                editor = createMixedScalarEditor(field, changed);
            else if (LuaMetadataType.Parse(field.Type).IsAny && getValueOptions(field).Count == 0
                && getAssetSubdirectory(field) is null && field.EditorKind == BlueprintVariableEditorKind.Default)
                editor = createMultipleTextEditor(field, false, changed);
            else if (field.Range is not null && getValueOptions(field).Count == 0 && !isProgressField(field)
                && (isIntegerType(field.Type, getFieldValue(field)) || isFloatType(field.Type, getFieldValue(field))))
                editor = createMultipleNumberEditor(field, false, changed);
            else
            {
                BlueprintVariableField single = field.Clone();
                single.IsMultiSelection = false;
                editor = createValueEditor(single, field.DisplayValue ?? getFieldValue(field), changed);
            }
        }
        editor.IsEnabled = !field.IsReadOnly && !field.IsMultiEditReadOnly;
        multipleFieldEditors.Add(new MultipleFieldEditor(root, field, editor));
        return editor;
    }

    private Control createMixedScalarEditor(BlueprintVariableField field, Action<JsonNode?, bool> changed)
    {
        JsonNode? value = getFieldValue(field);
        string type = getTypeName(field);
        LuaMetadataType declared = LuaMetadataType.Parse(type);
        LuaEnumDefinition? enumDefinition = declared.Kind == LuaMetadataTypeKind.Enum ? readEnum(declared) : null;
        IReadOnlyList<BlueprintVariableOption> options = enumDefinition?.Options ?? getValueOptions(field);
        if (enumDefinition?.ValueType?.Name == "string" && !options.Any(option => JsonNode.DeepEquals(option.Value, JsonValue.Create(string.Empty))))
            options = new[] { new BlueprintVariableOption(LocaleService.Get("GENERAL_DATA_PLACEHOLDER"), JsonValue.Create(string.Empty)) }.Concat(options).ToArray();
        if (options.Count > 0 || declared.Kind == LuaMetadataTypeKind.Enum)
        {
            ComboBox selector = new()
            {
                ItemsSource = options.Select(option => option.Clone()).ToArray(),
                PlaceholderText = LocaleService.Get("MULTIPLE_CHOICES"),
                HorizontalAlignment = HorizontalAlignment.Stretch,
                MinWidth = 180,
            };
            selector.SelectionChanged += (_, _) =>
            {
                if (selector.SelectedItem is BlueprintVariableOption selected)
                    changed(selected.Value?.DeepClone(), false);
            };
            return selector;
        }
        if (getAssetSubdirectory(field) is not null || field.EditorKind is not BlueprintVariableEditorKind.Default
            and not BlueprintVariableEditorKind.ObjectReference)
            return createMixedNativeEditor(field, changed);
        if (declared.IsAny)
            return createMultipleTextEditor(field, true, changed);
        if (isBoolType(type, value))
        {
            CheckBox box = new()
            {
                IsThreeState = true,
                IsChecked = null,
                Content = LocaleService.Get("MULTIPLE_CHOICES"),
                HorizontalAlignment = HorizontalAlignment.Left,
            };
            box.IsCheckedChanged += (_, _) =>
            {
                if (box.IsChecked is not bool next)
                    return;
                box.IsThreeState = false;
                box.Content = null;
                changed(JsonValue.Create(next), false);
            };
            return box;
        }
        if (isIntegerType(type, value) || isFloatType(type, value))
            return createMultipleNumberEditor(field, true, changed);
        if (isPrimitiveType(type) || field.EditorKind == BlueprintVariableEditorKind.ObjectReference)
            return createMultipleTextEditor(field, true, changed);
        return createMixedNativeEditor(field, changed);
    }

    private Control createMixedNativeEditor(BlueprintVariableField field, Action<JsonNode?, bool> changed)
    {
        BlueprintVariableField single = field.Clone();
        single.IsMultiSelection = false;
        if (getAssetSubdirectory(single) is string assetSubdirectory)
            return createPathEditor(single, null, assetSubdirectory, changed);
        TextBlock marker = new() { Text = LocaleService.Get("MULTIPLE_CHOICES"), Foreground = EditorTheme.Brush("TextMuted") };
        return new StackPanel
        {
            Spacing = 2,
            Children =
            {
                marker,
                createValueEditor(single, null, (next, refresh) =>
                {
                    marker.IsVisible = false;
                    changed(next, refresh);
                }),
            },
        };
    }

    private Control createMultipleInstanceVariableEditor(BlueprintVariableField field, Action<JsonNode?, bool> changed)
    {
        HashSet<string>? excluded = null;
        if (fields.Any(candidate => getMetadataString(candidate, "InstVarValue") == field.Name))
        {
            HashSet<GameVariableType> types = [];
            foreach (JsonNode? value in field.MultiEditValues)
            {
                if (!JsonScalar.TryGetString(value, out string name)
                    || gameVariables is null || !gameVariables.TryGet(name, out GameVariableDefinition? definition) || definition is null)
                    return EditorInputs.CreateReadOnlyTextBox(field.IsMixedValue ? LocaleService.Get("MULTIPLE_CHOICES") : getText(getFieldValue(field)));
                types.Add(definition.Type);
            }
            if (types.Count != 1)
                return EditorInputs.CreateReadOnlyTextBox(field.IsMixedValue ? LocaleService.Get("MULTIPLE_CHOICES") : getText(getFieldValue(field)));
            GameVariableType type = types.First();
            excluded = new HashSet<string>(gameVariables?.Variables.Where(variable => variable.Type != type)
                .Select(variable => variable.Name) ?? [], StringComparer.Ordinal);
        }
        Control editor = createInstanceVariableEditor(field, field.IsMixedValue ? null : getFieldValue(field), changed, excluded);
        if (field.IsMixedValue && editor is SearchableListPicker picker)
            picker.PlaceholderText = LocaleService.Get("MULTIPLE_CHOICES");
        return editor;
    }

    private Control createMultipleNumberEditor(BlueprintVariableField field, bool mixed, Action<JsonNode?, bool> changed)
    {
        JsonNode? value = getFieldValue(field);
        bool integer = isIntegerType(field.Type, value);
        if (!integer && Math.Abs(getDouble(value)) > (double)decimal.MaxValue)
            return createMultipleTextEditor(field, mixed, changed);
        BlueprintVariableRange? range = field.Range;
        decimal minimum = range is null ? integer ? long.MinValue : decimal.MinValue : (decimal)range.Minimum;
        decimal maximum = range is null ? integer ? long.MaxValue : decimal.MaxValue : (decimal)range.Maximum;
        NumericUpDown box = EditorInputs.CreateNumericUpDown(getDecimal(value), minimum, maximum,
            range is null ? integer ? 1 : 0.1m : (decimal)range.Step);
        box.FormatString = integer ? "0" : "G";
        if (mixed)
        {
            box.Value = null;
            box.PlaceholderText = LocaleService.Get("MULTIPLE_CHOICES");
        }
        attachHistory(box);
        box.ValueChanged += (_, args) =>
        {
            if (args.NewValue is not decimal next)
                return;
            changed(integer ? JsonValue.Create(decimal.ToInt64(next)) : JsonValue.Create(decimal.ToDouble(next)), false);
        };
        return box;
    }

    private Control createMultipleTextEditor(BlueprintVariableField field, bool mixed, Action<JsonNode?, bool> changed)
    {
        JsonNode? value = getFieldValue(field);
        string type = getTypeName(field);
        LuaMetadataType declared = LuaMetadataType.Parse(type);
        TextBox box = EditorInputs.CreateEditableTextBox(mixed ? string.Empty : getText(value));
        box.HorizontalAlignment = HorizontalAlignment.Stretch;
        box.PlaceholderText = mixed ? LocaleService.Get("MULTIPLE_CHOICES") : value is null ? "null" : null;
        attachHistory(box);
        IBrush? normalBorder = box.BorderBrush;
        void commitText()
        {
            string text = box.Text ?? string.Empty;
            JsonNode? next;
            if (declared.IsAny)
            {
                try
                {
                    next = JsonNode.Parse(text);
                }
                catch (JsonException)
                {
                    next = JsonValue.Create(text);
                }
                if (next is JsonArray or JsonObject)
                {
                    box.BorderBrush = Brushes.OrangeRed;
                    setTransientInput(box, true);
                    return;
                }
            }
            else if (isIntegerType(type, value))
            {
                if (!long.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out long number)
                    || field.Range is BlueprintVariableRange range && (number < range.Minimum || number > range.Maximum))
                {
                    box.BorderBrush = Brushes.OrangeRed;
                    setTransientInput(box, true);
                    return;
                }
                next = JsonValue.Create(number);
            }
            else if (isFloatType(type, value))
            {
                if (!double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double number)
                    || !double.IsFinite(number)
                    || field.Range is BlueprintVariableRange range && (number < range.Minimum || number > range.Maximum))
                {
                    box.BorderBrush = Brushes.OrangeRed;
                    setTransientInput(box, true);
                    return;
                }
                next = JsonValue.Create(number);
            }
            else
            {
                next = field.EditorKind == BlueprintVariableEditorKind.ObjectReference && string.IsNullOrWhiteSpace(text)
                    ? null : JsonValue.Create(text);
            }
            box.BorderBrush = normalBorder;
            box.PlaceholderText = null;
            setTransientInput(box, false);
            changed(next, false);
        }
        box.PropertyChanged += (_, args) =>
        {
            if (args.Property == TextBox.TextProperty)
                commitText();
        };
        box.KeyDown += (_, args) =>
        {
            if (args.Key == Key.Enter)
            {
                commitText();
                args.Handled = true;
            }
        };
        return box;
    }

    private sealed record MultipleFieldEditor(BlueprintVariableField Root, BlueprintVariableField Field, Control Editor);

    private void commitMultipleValue(BlueprintVariableField root, BlueprintVariableField field, JsonNode? value, bool refresh)
    {
        if (disposed || building || isReadOnly || root.IsReadOnly || root.IsMultiEditReadOnly
            || field.IsReadOnly || field.IsMultiEditReadOnly)
            return;
        if (!field.IsMixedValue && JsonNode.DeepEquals(getFieldValue(field), value))
            return;
        field.IsMixedValue = false;
        field.Value = value?.DeepClone();
        field.PreserveNullValue = value is null;
        root.Value = BlueprintMultiSelectionFields.Replace(getFieldValue(root), field.EditPath, value);
        values[root.Name] = root.Value?.DeepClone();
        ValueChanged?.Invoke(this, new BlueprintVariableValueChangedEventArgs(root.Name, value?.DeepClone(), refresh || root.Name == "material", field.EditPath));
    }
}
