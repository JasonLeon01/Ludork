using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class MetadataTypeSelector : UserControl
{
    private static readonly string[] ScalarTypes =
    [
        "string", "int", "float", "bool", "file", "enum",
        "sf.Vector2f", "sf.Vector2i", "sf.Vector2u", "sf.Vector3f", "sf.Vector3i", "sf.Vector3u",
        "sf.Color", "sf.IntRect",
    ];
    private readonly LuaEnumService enums;
    private readonly ComboBox type = new() { HorizontalAlignment = HorizontalAlignment.Stretch };
    private readonly ComboBox itemType = new() { HorizontalAlignment = HorizontalAlignment.Stretch };
    private readonly ComboBox enumModule = new() { HorizontalAlignment = HorizontalAlignment.Stretch };
    private readonly ComboBox enumValueType = new() { HorizontalAlignment = HorizontalAlignment.Stretch, ItemsSource = new[] { LocaleService.Get("ENUM_INFER_TYPE"), "string", "bool", "int", "float" }, SelectedIndex = 0 };
    private readonly ComboBox keyType = new() { HorizontalAlignment = HorizontalAlignment.Stretch, ItemsSource = new[] { "string", "enum" }, SelectedIndex = 0 };
    private readonly ComboBox keyModule = new() { HorizontalAlignment = HorizontalAlignment.Stretch };
    private readonly TextBlock keyLabel = new() { Text = LocaleService.Get("DICT_KEY_TYPE"), VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock keyKindLabel = new() { Text = LocaleService.Get("DICT_KEY_ENUM"), VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock valueTypeLabel = new() { Text = LocaleService.Get("ENUM_VALUE_TYPE"), VerticalAlignment = VerticalAlignment.Center };
    private string? selectedKeyModule;
    private readonly TextBlock itemLabel = new() { VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock kindLabel = new() { Text = "kind", VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock error = new() { Foreground = Brushes.OrangeRed, TextWrapping = TextWrapping.Wrap };
    private string? selectedModule;
    private bool updating;
    private string? readError;

    public MetadataTypeSelector(LuaEnumService enums)
    {
        this.enums = enums;
        foreach (string name in ScalarTypes.Take(6).Concat(["list", "dict"]).Concat(ScalarTypes.Skip(6)))
            type.Items.Add(name);
        foreach (string name in new[] { "any" }.Concat(ScalarTypes))
            itemType.Items.Add(name);
        type.SelectedItem = "string";
        itemType.SelectedItem = "any";
        Grid grid = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*"), RowDefinitions = new RowDefinitions("Auto,Auto,Auto,Auto,Auto,Auto,Auto"), RowSpacing = 8, ColumnSpacing = 12 };
        Grid.SetColumnSpan(type, 2);
        grid.Children.Add(type);
        Grid.SetRow(itemLabel, 1);
        grid.Children.Add(itemLabel);
        Grid.SetRow(itemType, 1);
        Grid.SetColumn(itemType, 1);
        grid.Children.Add(itemType);
        Grid.SetRow(kindLabel, 2);
        grid.Children.Add(kindLabel);
        Grid.SetRow(enumModule, 2);
        Grid.SetColumn(enumModule, 1);
        grid.Children.Add(enumModule);
        addRow(grid, valueTypeLabel, enumValueType, 3);
        addRow(grid, keyLabel, keyType, 4);
        addRow(grid, keyKindLabel, keyModule, 5);
        Grid.SetRow(error, 6);
        Grid.SetColumnSpan(error, 2);
        grid.Children.Add(error);
        Content = grid;
        type.SelectionChanged += (_, _) => selectionChanged();
        itemType.SelectionChanged += (_, _) => selectionChanged();
        enumValueType.SelectionChanged += (_, _) => selectionChanged();
        keyType.SelectionChanged += (_, _) => selectionChanged();
        keyModule.DropDownOpened += (_, _) => refreshModules();
        keyModule.SelectionChanged += (_, _) =>
        {
            if (updating)
                return;
            selectedKeyModule = keyModule.SelectedItem as string;
            selectionChanged();
        };
        enumModule.DropDownOpened += (_, _) => refreshModules();
        enumModule.SelectionChanged += (_, _) =>
        {
            if (updating)
                return;
            selectedModule = enumModule.SelectedItem as string;
            if (selectedModule is not null)
            {
                updating = true;
                enumValueType.SelectedItem = enums.IsProjectModule(selectedModule) ? "string" : LocaleService.Get("ENUM_INFER_TYPE");
                updating = false;
            }
            selectionChanged();
        };
        AttachedToVisualTree += (_, _) => refreshModules();
        updateVisibility();
    }

    public event EventHandler? SelectionChanged;
    public string SelectedCategory => type.SelectedItem as string ?? "string";

    public void SetType(LuaMetadataType schema)
    {
        updating = true;
        LuaMetadataType element = schema;
        string category;
        if (schema.Kind is LuaMetadataTypeKind.List or LuaMetadataTypeKind.Dictionary)
        {
            category = schema.Kind == LuaMetadataTypeKind.List ? "list" : "dict";
            element = schema.Arguments[schema.Kind == LuaMetadataTypeKind.List ? 0 : 1];
            select(itemType, element.Kind == LuaMetadataTypeKind.Enum ? "enum" : element.ToString());
        }
        else
        {
            category = schema.Kind == LuaMetadataTypeKind.Enum ? "enum" : schema.ToString();
        }
        selectedModule = element.Kind == LuaMetadataTypeKind.Enum ? element.Name : null;
        enumValueType.SelectedItem = element.EnumValueType?.Name ?? LocaleService.Get("ENUM_INFER_TYPE");
        LuaMetadataType? key = schema.Kind == LuaMetadataTypeKind.Dictionary ? schema.Arguments[0] : null;
        keyType.SelectedItem = key?.Kind == LuaMetadataTypeKind.Enum ? "enum" : "string";
        selectedKeyModule = key?.Kind == LuaMetadataTypeKind.Enum ? key.Name : null;
        select(type, category);
        updating = false;
        updateVisibility();
        refreshModules();
    }

    public bool TryGetType(out LuaMetadataType? schema, out string? diagnostic)
    {
        schema = null;
        diagnostic = null;
        bool container = SelectedCategory is "list" or "dict";
        string selected = container ? itemType.SelectedItem as string ?? "any" : SelectedCategory;
        JsonNode value;
        if (selected == "enum")
        {
            if (string.IsNullOrEmpty(selectedModule))
            {
                diagnostic = readError ?? LocaleService.Get("ENUM_SELECT_KIND");
                return false;
            }
            JsonObject enumeration = new() { ["enum"] = selectedModule };
            string underlying = enumValueType.SelectedItem as string ?? "";
            if (enumValueType.SelectedIndex > 0)
                enumeration["valueType"] = underlying;
            value = enumeration;
        }
        else
        {
            value = LuaMetadataType.Parse(selected).ToSchema();
        }
        JsonNode declaration = value;
        if (container)
        {
            JsonObject collection = new() { [SelectedCategory] = value };
            if (SelectedCategory == "dict" && keyType.SelectedItem as string == "enum")
            {
                if (string.IsNullOrEmpty(selectedKeyModule))
                {
                    diagnostic = readError ?? LocaleService.Get("ENUM_SELECT_KIND");
                    return false;
                }
                collection["key"] = new JsonObject { ["enum"] = selectedKeyModule, ["valueType"] = "string" };
            }
            declaration = collection;
        }
        schema = LuaMetadataType.Parse(declaration);
        return true;
    }

    private static void addRow(Grid grid, TextBlock label, Control input, int row)
    {
        Grid.SetRow(label, row);
        Grid.SetRow(input, row);
        Grid.SetColumn(input, 1);
        grid.Children.Add(label);
        grid.Children.Add(input);
    }

    private static void select(ComboBox box, string value)
    {
        if (!box.Items.Contains(value))
            box.Items.Add(value);
        box.SelectedItem = value;
    }

    private void selectionChanged()
    {
        if (updating)
            return;
        updateVisibility();
        refreshModules();
        SelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    private void updateVisibility()
    {
        bool container = SelectedCategory is "list" or "dict";
        itemType.IsVisible = container;
        itemLabel.IsVisible = container;
        itemLabel.Text = LocaleService.Get(SelectedCategory == "list" ? "ITEM_TYPE" : "VALUE_TYPE");
        bool enumeration = (container ? itemType.SelectedItem as string : SelectedCategory) == "enum";
        kindLabel.IsVisible = enumeration;
        enumModule.IsVisible = enumeration;
        valueTypeLabel.IsVisible = enumeration;
        enumValueType.IsVisible = enumeration;
        keyLabel.IsVisible = SelectedCategory == "dict";
        keyType.IsVisible = keyLabel.IsVisible;
        keyKindLabel.IsVisible = keyLabel.IsVisible && keyType.SelectedItem as string == "enum";
        keyModule.IsVisible = keyKindLabel.IsVisible;
        error.IsVisible = (enumeration || keyModule.IsVisible) && readError is not null;
    }

    private void refreshModules()
    {
        if (!enumModule.IsVisible && !keyModule.IsVisible)
            return;
        updating = true;
        IReadOnlyList<string> modules;
        try
        {
            modules = enums.EnumerateModules();
            readError = modules.Count == 0 ? LocaleService.Get("ENUM_NO_MODULES") : null;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            modules = [];
            readError = exception.Message;
        }
        string[] keyModules = keyModule.IsVisible
            ? modules.Where(module => enums.Read(LuaMetadataType.Parse(new JsonObject
            {
                ["enum"] = module,
                ["valueType"] = "string",
            })).IsValid).ToArray()
            : [];
        keyModule.ItemsSource = keyModules;
        keyModule.SelectedItem = keyModules.Contains(selectedKeyModule, StringComparer.Ordinal) ? selectedKeyModule : null;
        keyModule.PlaceholderText = selectedKeyModule ?? LocaleService.Get("ENUM_SELECT_KIND");
        enumModule.ItemsSource = modules;
        enumModule.SelectedItem = modules.Contains(selectedModule, StringComparer.Ordinal) ? selectedModule : null;
        enumModule.PlaceholderText = selectedModule ?? LocaleService.Get("ENUM_SELECT_KIND");
        error.Text = readError;
        error.IsVisible = readError is not null;
        updating = false;
    }
}
