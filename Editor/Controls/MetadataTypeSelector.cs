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
    private readonly TextBlock itemLabel = new() { VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock kindLabel = new() { Text = "kind", VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock error = new() { Foreground = Brushes.OrangeRed, TextWrapping = TextWrapping.Wrap };
    private string? selectedModule;
    private bool updating;
    private string? readError;

    public MetadataTypeSelector(string projectPath)
    {
        enums = new LuaEnumService(projectPath);
        foreach (string name in ScalarTypes.Take(6).Concat(["list", "dict"]).Concat(ScalarTypes.Skip(6)))
            type.Items.Add(name);
        foreach (string name in new[] { "any" }.Concat(ScalarTypes))
            itemType.Items.Add(name);
        type.SelectedItem = "string";
        itemType.SelectedItem = "any";
        Grid grid = new() { ColumnDefinitions = new ColumnDefinitions("Auto,*"), RowDefinitions = new RowDefinitions("Auto,Auto,Auto,Auto"), RowSpacing = 8, ColumnSpacing = 12 };
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
        Grid.SetRow(error, 3);
        Grid.SetColumnSpan(error, 2);
        grid.Children.Add(error);
        Content = grid;
        type.SelectionChanged += (_, _) => selectionChanged();
        itemType.SelectionChanged += (_, _) => selectionChanged();
        enumModule.DropDownOpened += (_, _) => refreshModules();
        enumModule.SelectionChanged += (_, _) =>
        {
            if (updating)
                return;
            selectedModule = enumModule.SelectedItem as string;
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
            value = new JsonObject { ["enum"] = selectedModule };
        }
        else
        {
            value = LuaMetadataType.Parse(selected).ToSchema();
        }
        schema = LuaMetadataType.Parse(container ? new JsonObject { [SelectedCategory] = value } : value);
        return true;
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
        error.IsVisible = enumeration && readError is not null;
    }

    private void refreshModules()
    {
        if (!enumModule.IsVisible)
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
        enumModule.ItemsSource = modules;
        enumModule.SelectedItem = modules.Contains(selectedModule, StringComparer.Ordinal) ? selectedModule : null;
        enumModule.PlaceholderText = selectedModule ?? LocaleService.Get("ENUM_SELECT_KIND");
        error.Text = readError;
        error.IsVisible = readError is not null;
        updating = false;
    }
}
