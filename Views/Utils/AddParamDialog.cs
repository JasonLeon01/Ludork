using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class AddParamDialog : Window
{
    private static readonly string[] ParamTypes =
    [
        "string",
        "int",
        "float",
        "bool",
        "file",
        "list",
        "dict",
        "sf.Vector2f",
        "sf.Vector2i",
        "sf.Vector2u",
        "sf.Vector3f",
        "sf.Vector3i",
        "sf.Vector3u",
        "sf.Color",
        "sf.IntRect",
    ];
    private static readonly string[] ContainerItemTypes =
    [
        "any",
        "string",
        "int",
        "float",
        "bool",
        "file",
        "sf.Vector2f",
        "sf.Vector2i",
        "sf.Vector2u",
        "sf.Vector3f",
        "sf.Vector3i",
        "sf.Vector3u",
        "sf.Color",
        "sf.IntRect",
    ];

    private readonly IEnumerable<string> existingParams;
    private readonly TextBox nameBox;
    private readonly TextBox commentBox;
    private readonly ComboBox typeCombo;
    private readonly TextBlock containerItemTypeLabel;
    private readonly ComboBox containerItemTypeCombo;
    private readonly TextBlock defaultLabel;
    private readonly TextBox defaultBox;
    private readonly TextBlock typeTipBlock;
    private readonly TextBlock defaultTipBlock;
    private readonly TextBlock errorText;

    private AddParamDialog(
        IEnumerable<string> existingParams,
        GeneralDataParamCreation? initialValue)
    {
        this.existingParams = existingParams;
        Title = LocaleService.Get(initialValue is null ? "ADD_PARAM" : "EDIT_PARAM");
        Width = 480;
        Height = 400;
        MinWidth = 380;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.FromRgb(43, 43, 43));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        nameBox = EditorInputs.CreateEditableTextBox();
        commentBox = EditorInputs.CreateEditableTextBox();

        typeCombo = new ComboBox { HorizontalAlignment = HorizontalAlignment.Stretch };
        foreach (string t in ParamTypes)
            typeCombo.Items.Add(t);
        typeCombo.SelectedIndex = 0;

        containerItemTypeLabel = new TextBlock
        {
            VerticalAlignment = VerticalAlignment.Center,
        };
        containerItemTypeCombo = new ComboBox { HorizontalAlignment = HorizontalAlignment.Stretch };
        foreach (string t in ContainerItemTypes)
            containerItemTypeCombo.Items.Add(t);
        containerItemTypeCombo.SelectedIndex = 0;

        defaultBox = EditorInputs.CreateEditableTextBox();
        defaultLabel = new TextBlock
        {
            VerticalAlignment = VerticalAlignment.Center,
        };
        typeTipBlock = new TextBlock
        {
            TextWrapping = TextWrapping.Wrap,
            FontSize = 11,
            Foreground = new SolidColorBrush(Color.FromRgb(160, 160, 160)),
        };
        defaultTipBlock = new TextBlock
        {
            TextWrapping = TextWrapping.Wrap,
            FontSize = 11,
            Foreground = new SolidColorBrush(Color.FromRgb(160, 160, 160)),
        };
        errorText = new TextBlock
        {
            Foreground = new SolidColorBrush(Color.FromRgb(200, 80, 80)),
            TextWrapping = TextWrapping.Wrap,
        };

        typeCombo.SelectionChanged += (_, _) => updateTypeState();

        if (initialValue is not null)
        {
            nameBox.Text = initialValue.Name;
            commentBox.Text = initialValue.Comment;
            typeCombo.SelectedItem = initialValue.Type;
            containerItemTypeCombo.SelectedItem = initialValue.ItemType
                ?? initialValue.ValueType
                ?? "any";
            defaultBox.Text = initialValue.DefaultText;
        }

        Grid form = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,12,*"),
            RowSpacing = 8,
        };
        addRow(form, 0, LocaleService.Get("PARAM_NAME"), nameBox);
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        addRow(form, 1, LocaleService.Get("PARAM_COMMENT"), commentBox);
        addRow(form, 2, LocaleService.Get("PARAM_TYPE"), typeCombo);
        addRow(form, 3, containerItemTypeLabel, containerItemTypeCombo);
        addRow(form, 4, defaultLabel, defaultBox);
        updateTypeState();

        Button confirm = new() { Content = LocaleService.Get("CONFIRM"), MinWidth = 80 };
        confirm.Click += onConfirm;
        Button cancel = new() { Content = LocaleService.Get("CANCEL"), MinWidth = 80 };
        cancel.Click += (_, _) => Close(null);

        StackPanel buttons = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
        };
        buttons.Children.Add(confirm);
        buttons.Children.Add(cancel);

        StackPanel content = new() { Margin = new Thickness(20), Spacing = 10 };
        content.Children.Add(form);
        content.Children.Add(typeTipBlock);
        content.Children.Add(defaultTipBlock);
        content.Children.Add(errorText);
        content.Children.Add(buttons);
        Content = content;

        Opened += (_, _) =>
        {
            nameBox.Focus();
            nameBox.SelectAll();
        };
    }

    private void addRow(Grid form, int row, string label, Control editor)
    {
        addRow(
            form,
            row,
            new TextBlock
            {
                Text = label,
                VerticalAlignment = VerticalAlignment.Center,
            },
            editor);
    }

    private void addRow(Grid form, int row, TextBlock label, Control editor)
    {
        while (form.RowDefinitions.Count <= row)
            form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        Grid.SetRow(label, row);
        Grid.SetColumn(label, 0);
        form.Children.Add(label);
        Grid.SetRow(editor, row);
        Grid.SetColumn(editor, 2);
        form.Children.Add(editor);
    }

    private void updateTypeState()
    {
        string t = typeCombo.SelectedItem as string ?? "string";
        bool list = t == "list";
        bool dict = t == "dict";
        containerItemTypeLabel.Text = LocaleService.Get(list ? "ITEM_TYPE" : "VALUE_TYPE");
        containerItemTypeLabel.IsVisible = list || dict;
        containerItemTypeCombo.IsVisible = list || dict;
        bool sfType = t.StartsWith("sf.", System.StringComparison.Ordinal);
        defaultBox.IsEnabled = !list && !dict && !sfType;
        defaultLabel.Text = LocaleService.Get(t == "file" ? "ASSET_SELECTION_ROOT" : "DEFAULT_VALUE");
        string tipType = sfType ? "SF" : t.ToUpperInvariant();
        typeTipBlock.Text = LocaleService.Get("GENERAL_DATA_TYPE_TIP_" + tipType);
        defaultTipBlock.Text = LocaleService.Get("GENERAL_DATA_DEFAULT_TIP_" + tipType);
    }

    public static Task<GeneralDataParamCreation?> ShowAsync(
        Window owner,
        IEnumerable<string> existingParams)
    {
        return new AddParamDialog(existingParams, null).ShowDialog<GeneralDataParamCreation?>(owner);
    }

    public static Task<GeneralDataParamCreation?> ShowEditAsync(
        Window owner,
        IEnumerable<string> existingParams,
        GeneralDataParamCreation initialValue)
    {
        return new AddParamDialog(existingParams, initialValue).ShowDialog<GeneralDataParamCreation?>(owner);
    }

    private void onConfirm(object? sender, RoutedEventArgs args)
    {
        string name = nameBox.Text?.Trim() ?? string.Empty;
        errorText.Text = string.Empty;
        if (string.IsNullOrWhiteSpace(name))
        {
            errorText.Text = LocaleService.Get("ADD_EMPTY");
            return;
        }
        if (existingParams.Contains(name, System.StringComparer.Ordinal))
        {
            errorText.Text = LocaleService.Get("PARAM_EXISTS");
            return;
        }
        string type = typeCombo.SelectedItem as string ?? "string";
        string containerItemType = containerItemTypeCombo.SelectedItem as string ?? "string";
        string defaultText = defaultBox.Text ?? string.Empty;
        string comment = commentBox.Text?.Trim() ?? string.Empty;
        Close(new GeneralDataParamCreation(
            name,
            type,
            type == "list" ? containerItemType : null,
            type == "dict" ? containerItemType : null,
            defaultText,
            comment));
    }
}

public sealed record GeneralDataParamCreation(
    string Name,
    string Type,
    string? ItemType,
    string? ValueType,
    string DefaultText,
    string Comment);
