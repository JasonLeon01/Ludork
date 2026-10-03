using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class AddBlueprintAttributeDialog : Window
{
    private readonly HashSet<string> existingNames;
    private readonly Func<string, BlueprintFieldMetadata?>? resolveDeclaredField;
    private readonly TextBox name = EditorInputs.CreateEditableTextBox();
    private readonly MetadataTypeSelector type;
    private readonly MetadataValueEditor value;
    private readonly TextBox fileBase = EditorInputs.CreateEditableTextBox();
    private readonly StackPanel baseRow = new() { Spacing = 4 };
    private readonly TextBlock declaredHint = new() { TextWrapping = TextWrapping.Wrap, Foreground = EditorTheme.Brush("TextMuted") };
    private readonly TextBlock error = new() { Foreground = Brushes.OrangeRed, TextWrapping = TextWrapping.Wrap };
    private BlueprintFieldMetadata? declaration;
    private LuaMetadataType customType = LuaMetadataType.Parse("string");
    private string customBase = string.Empty;
    private bool updating;

    private AddBlueprintAttributeDialog(
        string projectPath,
        IEnumerable<string> existingNames,
        Func<string, BlueprintFieldMetadata?>? resolveDeclaredField)
    {
        this.existingNames = new HashSet<string>(existingNames, StringComparer.Ordinal);
        this.resolveDeclaredField = resolveDeclaredField;
        type = new MetadataTypeSelector(projectPath);
        value = new MetadataValueEditor(projectPath);
        Title = LocaleService.Get("ADD_ATTRIBUTE");
        Width = 560;
        Height = 540;
        MinWidth = 440;
        MinHeight = 340;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Surface");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        baseRow.Children.Add(new TextBlock { Text = LocaleService.Get("ASSET_SELECTION_ROOT") });
        baseRow.Children.Add(fileBase);
        StackPanel fields = new() { Spacing = 10 };
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("PARAM_NAME") });
        fields.Children.Add(name);
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("PARAM_TYPE") });
        fields.Children.Add(type);
        fields.Children.Add(declaredHint);
        fields.Children.Add(baseRow);
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("ATTRIBUTE_INITIAL_VALUE") });
        fields.Children.Add(value);
        fields.Children.Add(error);
        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), MinWidth = 80 };
        confirm.Click += (_, _) => confirmCreation();
        Button cancel = new() { Content = LocaleService.Get("CANCEL"), MinWidth = 80 };
        cancel.Click += (_, _) => Close(null);
        StackPanel buttons = new() { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 8, Children = { confirm, cancel } };
        Grid content = new() { Margin = new Thickness(20), RowDefinitions = new RowDefinitions("*,Auto"), RowSpacing = 12 };
        content.Children.Add(new ScrollViewer { Content = fields });
        Grid.SetRow(buttons, 1);
        content.Children.Add(buttons);
        Content = content;
        name.TextChanged += (_, _) => updateDeclaration();
        type.SelectionChanged += (_, _) => updateType();
        fileBase.TextChanged += (_, _) => updateFileBase();
        Opened += (_, _) => name.Focus();
        Closed += (_, _) => value.Dispose();
        updateType();
    }

    public static Task<BlueprintAttributeCreation?> ShowAsync(
        Window owner,
        string projectPath,
        IEnumerable<string> existingNames,
        Func<string, BlueprintFieldMetadata?>? resolveDeclaredField = null)
    {
        return new AddBlueprintAttributeDialog(projectPath, existingNames, resolveDeclaredField)
            .ShowDialog<BlueprintAttributeCreation?>(owner);
    }

    private void updateDeclaration()
    {
        BlueprintFieldMetadata? next = resolveDeclaredField?.Invoke(name.Text?.Trim() ?? string.Empty);
        if (next is null && declaration is null
            || next is not null && declaration is not null && next.Name == declaration.Name && next.Type == declaration.Type)
            return;
        declaration = next;
        updating = true;
        type.IsEnabled = declaration is null;
        fileBase.IsReadOnly = declaration is not null;
        if (declaration is not null)
            EditorInputs.ApplyReadOnly(fileBase);
        else
            EditorInputs.ApplyEditable(fileBase);
        declaredHint.IsVisible = declaration is not null;
        declaredHint.Text = declaration is null ? string.Empty : LocaleService.Get("ATTRIBUTE_DECLARED_TYPE");
        LuaMetadataType schema = declaration?.Type.WithDefaultModule(declaration.DeclaringType.ModuleName).Schema ?? customType;
        type.SetType(schema);
        string? declaredBase = getString(declaration?.Meta["PathVars"]);
        fileBase.Text = declaration is null ? customBase : declaredBase ?? string.Empty;
        baseRow.IsVisible = schema.Name == "file" || declaredBase is not null;
        JsonObject meta = buildMeta();
        if (declaration?.HasDefaultValue == true)
            value.SetValue(schema, declaration.DefaultValue, meta);
        else
            value.Reset(schema, meta);
        value.IsVisible = true;
        error.Text = string.Empty;
        updating = false;
    }

    private void updateType()
    {
        if (updating || declaration is not null)
            return;
        baseRow.IsVisible = type.SelectedCategory == "file";
        if (!type.TryGetType(out LuaMetadataType? schema, out string? diagnostic))
        {
            value.IsVisible = false;
            error.Text = diagnostic;
            return;
        }
        customType = schema!;
        value.IsVisible = true;
        value.Reset(customType, buildMeta());
        error.Text = string.Empty;
    }

    private void updateFileBase()
    {
        if (updating || declaration is not null)
            return;
        customBase = fileBase.Text?.Trim() ?? string.Empty;
        if (type.TryGetType(out LuaMetadataType? schema, out _))
            value.SetValue(schema!, value.Value, buildMeta());
    }

    private JsonObject buildMeta()
    {
        JsonObject result = declaration?.Meta.DeepClone() as JsonObject ?? [];
        if (type.SelectedCategory == "file" && !result.ContainsKey("PathVars"))
        {
            string root = fileBase.Text?.Trim() ?? string.Empty;
            result["PathVars"] = root.Length == 0 ? GameAssetPath.Root
                : root.StartsWith("/", StringComparison.Ordinal) ? root : GameAssetPath.Root + "/" + root;
        }
        return result;
    }

    private void confirmCreation()
    {
        updateDeclaration();
        string fieldName = name.Text?.Trim() ?? string.Empty;
        if (fieldName.Length == 0 || existingNames.Contains(fieldName))
        {
            error.Text = LocaleService.Get(fieldName.Length == 0 ? "ADD_EMPTY" : "PARAM_EXISTS");
            return;
        }
        if (!type.TryGetType(out LuaMetadataType? schema, out string? diagnostic) || !value.TryValidate(out diagnostic))
        {
            error.Text = diagnostic;
            return;
        }
        string? root = baseRow.IsVisible ? fileBase.Text?.Trim() : null;
        if (declaration is null && schema!.Name == "file" && !GameAssetPath.IsValidBaseHint(root))
        {
            error.Text = LocaleService.Get("ATTRIBUTE_INVALID_FILE_BASE");
            return;
        }
        Close(new BlueprintAttributeCreation(fieldName, schema!, value.Value, string.IsNullOrEmpty(root) ? null : root));
    }

    private static string? getString(JsonNode? node)
    {
        return node is JsonValue scalar && scalar.TryGetValue(out string? text) ? text : null;
    }
}
