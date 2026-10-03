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

public sealed class AddParamDialog : Window
{
    private readonly GeneralDataParamCreation? initialValue;
    private readonly HashSet<string> existingParams;
    private readonly TextBox name = EditorInputs.CreateEditableTextBox();
    private readonly TextBox comment = EditorInputs.CreateEditableTextBox();
    private readonly TextBox fileBase = EditorInputs.CreateEditableTextBox();
    private readonly MetadataTypeSelector type;
    private readonly MetadataValueEditor value;
    private readonly StackPanel baseRow = new() { Spacing = 4 };
    private readonly StackPanel defaultRow = new() { Spacing = 4 };
    private readonly TextBlock typeTip = new() { TextWrapping = TextWrapping.Wrap, FontSize = 11, Foreground = EditorTheme.Brush("TextMuted") };
    private readonly TextBlock error = new() { Foreground = Brushes.OrangeRed, TextWrapping = TextWrapping.Wrap };

    private AddParamDialog(
        string projectPath,
        IEnumerable<string> existingParams,
        GeneralDataParamCreation? initialValue)
    {
        this.initialValue = initialValue;
        this.existingParams = new HashSet<string>(existingParams, StringComparer.Ordinal);
        type = new MetadataTypeSelector(projectPath);
        value = new MetadataValueEditor(projectPath);
        Title = LocaleService.Get(initialValue is null ? "ADD_PARAM" : "EDIT_PARAM");
        Width = 560;
        Height = 580;
        MinWidth = 440;
        MinHeight = 340;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = EditorTheme.Brush("Surface");
        FontFamily = EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        StackPanel fields = new() { Spacing = 10 };
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("PARAM_NAME") });
        fields.Children.Add(name);
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("PARAM_COMMENT") });
        fields.Children.Add(comment);
        fields.Children.Add(new TextBlock { Text = LocaleService.Get("PARAM_TYPE") });
        fields.Children.Add(type);
        fields.Children.Add(typeTip);
        baseRow.Children.Add(new TextBlock { Text = LocaleService.Get("ASSET_SELECTION_ROOT") });
        baseRow.Children.Add(fileBase);
        fields.Children.Add(baseRow);
        defaultRow.Children.Add(new TextBlock { Text = LocaleService.Get("DEFAULT_VALUE") });
        defaultRow.Children.Add(value);
        fields.Children.Add(defaultRow);
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
        type.SelectionChanged += (_, _) => updateType();
        Opened += (_, _) =>
        {
            name.Focus();
            name.SelectAll();
        };
        Closed += (_, _) => value.Dispose();
        if (initialValue is not null)
        {
            name.Text = initialValue.Name;
            comment.Text = initialValue.Comment;
            LuaMetadataType schema = initialValue.Type switch
            {
                "list" => LuaMetadataType.Parse(new JsonObject { ["list"] = LuaMetadataType.Parse(initialValue.ItemType ?? "any").ToSchema() }),
                "dict" => LuaMetadataType.Parse(new JsonObject { ["dict"] = LuaMetadataType.Parse(initialValue.ValueType ?? "any").ToSchema() }),
                _ => LuaMetadataType.Parse(initialValue.Type),
            };
            type.SetType(schema);
            updateType();
            JsonObject definition = GeneralDataParameterSchema.BuildParamDefinition(initialValue, new LuaEnumService(projectPath).Read);
            value.SetValue(schema, definition["defaultValue"]);
            if (initialValue.Type == "file")
                fileBase.Text = initialValue.DefaultText;
        }
        else
        {
            updateType();
        }
    }

    public static Task<GeneralDataParamCreation?> ShowAsync(
        Window owner,
        IEnumerable<string> existingParams,
        string projectPath)
    {
        return new AddParamDialog(projectPath, existingParams, null).ShowDialog<GeneralDataParamCreation?>(owner);
    }

    public static Task<GeneralDataParamCreation?> ShowEditAsync(
        Window owner,
        IEnumerable<string> existingParams,
        GeneralDataParamCreation initialValue,
        string projectPath)
    {
        return new AddParamDialog(projectPath, existingParams, initialValue).ShowDialog<GeneralDataParamCreation?>(owner);
    }

    private void updateType()
    {
        string category = type.SelectedCategory;
        baseRow.IsVisible = category == "file";
        defaultRow.IsVisible = category != "file";
        string tip = category.StartsWith("sf.", StringComparison.Ordinal) ? "SF"
            : category.StartsWith("Union[", StringComparison.Ordinal) ? "UNION" : category.ToUpperInvariant();
        string tipKey = "GENERAL_DATA_TYPE_TIP_" + tip;
        typeTip.Text = LocaleService.Get(tipKey);
        typeTip.IsVisible = typeTip.Text != tipKey;
        if (!type.TryGetType(out LuaMetadataType? schema, out string? diagnostic))
        {
            value.IsVisible = false;
            error.Text = diagnostic;
            return;
        }
        value.IsVisible = true;
        value.Reset(schema!);
        error.Text = string.Empty;
    }

    private void confirmCreation()
    {
        string fieldName = name.Text?.Trim() ?? string.Empty;
        if (fieldName.Length == 0 || existingParams.Contains(fieldName))
        {
            error.Text = LocaleService.Get(fieldName.Length == 0 ? "ADD_EMPTY" : "PARAM_EXISTS");
            return;
        }
        if (!type.TryGetType(out LuaMetadataType? schema, out string? diagnostic) || !value.TryValidate(out diagnostic))
        {
            error.Text = diagnostic;
            return;
        }
        if (type.SelectedCategory == "file" && !GameAssetPath.IsValidBaseHint(fileBase.Text?.Trim()))
        {
            error.Text = LocaleService.Get("ATTRIBUTE_INVALID_FILE_BASE");
            return;
        }
        string category = schema!.Kind == LuaMetadataTypeKind.List ? "list"
            : schema.Kind == LuaMetadataTypeKind.Dictionary ? "dict" : schema.ToString();
        if (initialValue is not null && initialValue.Type is not ("list" or "dict")
            && LuaMetadataType.Parse(initialValue.Type).ToString() == schema.ToString())
        {
            category = initialValue.Type;
        }
        string defaultText = category == "file" ? fileBase.Text?.Trim() ?? string.Empty
            : schema.Kind == LuaMetadataTypeKind.Named && schema.Name == "string" ? value.Value?.GetValue<string>() ?? string.Empty
            : value.Value?.ToJsonString() ?? "null";
        Close(new GeneralDataParamCreation(
            fieldName,
            category,
            category == "list" ? schema.Arguments[0].ToString() : null,
            category == "dict" ? schema.Arguments[1].ToString() : null,
            defaultText,
            comment.Text?.Trim() ?? string.Empty));
    }
}
