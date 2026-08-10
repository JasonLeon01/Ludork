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
    private static readonly string[] ParamTypes = ["string", "int", "float", "bool", "file", "list", "dict"];

    private readonly IEnumerable<string> existingParams;
    private readonly TextBox nameBox;
    private readonly ComboBox typeCombo;
    private readonly TextBox defaultBox;
    private readonly TextBlock typeTipBlock;
    private readonly TextBlock defaultTipBlock;
    private readonly TextBlock errorText;

    private AddParamDialog(IEnumerable<string> existingParams)
    {
        this.existingParams = existingParams;
        Title = LocaleService.Get("ADD_PARAM");
        Width = 480;
        Height = 320;
        MinWidth = 380;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.FromRgb(43, 43, 43));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        nameBox = EditorInputs.CreateEditableTextBox();

        typeCombo = new ComboBox { HorizontalAlignment = HorizontalAlignment.Stretch };
        foreach (string t in ParamTypes)
            typeCombo.Items.Add(t);
        typeCombo.SelectedIndex = 0;

        defaultBox = EditorInputs.CreateEditableTextBox();
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

        typeCombo.SelectionChanged += (_, _) => updateTips();
        updateTips();

        Grid form = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,12,*"),
            RowSpacing = 8,
        };
        addRow(form, 0, LocaleService.Get("PARAM_NAME"), nameBox);
        form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        addRow(form, 1, LocaleService.Get("PARAM_TYPE"), typeCombo);
        addRow(form, 2, LocaleService.Get("DEFAULT_VALUE"), defaultBox);

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
        while (form.RowDefinitions.Count <= row)
            form.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
        TextBlock lbl = new()
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetRow(lbl, row);
        Grid.SetColumn(lbl, 0);
        form.Children.Add(lbl);
        Grid.SetRow(editor, row);
        Grid.SetColumn(editor, 2);
        form.Children.Add(editor);
    }

    private void updateTips()
    {
        string t = typeCombo.SelectedItem as string ?? "string";
        typeTipBlock.Text = LocaleService.Get("GENERAL_DATA_TYPE_TIP_" + t.ToUpperInvariant());
        defaultTipBlock.Text = LocaleService.Get("GENERAL_DATA_DEFAULT_TIP_" + t.ToUpperInvariant());
    }

    public static Task<(string name, string type, string defaultText)?> ShowAsync(
        Window owner,
        IEnumerable<string> existingParams)
    {
        return new AddParamDialog(existingParams).ShowDialog<(string, string, string)?>(owner);
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
        string defaultText = defaultBox.Text ?? string.Empty;
        Close((name, type, defaultText));
    }
}
