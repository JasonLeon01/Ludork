using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed record GameVariableCreation(string Name, string TypeName);

public sealed class AddGameVariableDialog : Window
{
    private readonly HashSet<string> existingNames;
    private readonly TextBox nameBox;
    private readonly ComboBox typeBox;
    private readonly TextBlock errorText;

    private AddGameVariableDialog(
        IEnumerable<string> existingNames,
        IEnumerable<string> typeNames)
    {
        this.existingNames = new HashSet<string>(existingNames, StringComparer.Ordinal);
        Title = LocaleService.Get("NEW_GAME_VARIABLE");
        Width = 440;
        Height = 220;
        MinWidth = 360;
        MinHeight = 200;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.FromRgb(43, 43, 43));
        FontFamily = FontFamily.Parse(
            "avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);

        nameBox = EditorInputs.CreateEditableTextBox();
        typeBox = new ComboBox
        {
            ItemsSource = typeNames.ToArray(),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            MinHeight = EditorInputs.FieldMinHeight,
        };
        typeBox.SelectedIndex = 0;
        errorText = new TextBlock
        {
            Foreground = new SolidColorBrush(Color.Parse("#c85050")),
            TextWrapping = TextWrapping.Wrap,
        };

        Grid form = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,12,*"),
            RowDefinitions = new RowDefinitions("Auto,8,Auto"),
        };
        addRow(form, 0, LocaleService.Get("GAME_VARIABLE_NAME"), nameBox);
        addRow(form, 2, LocaleService.Get("GAME_VARIABLE_TYPE"), typeBox);

        Button confirm = new()
        {
            Content = LocaleService.Get("CONFIRM"),
            MinWidth = 80,
        };
        confirm.Click += (_, _) => confirmSelection();
        Button cancel = new()
        {
            Content = LocaleService.Get("CANCEL"),
            MinWidth = 80,
        };
        cancel.Click += (_, _) => Close(null);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children =
            {
                confirm,
                cancel,
            },
        };

        Grid content = new()
        {
            Margin = new Thickness(20),
            RowDefinitions = new RowDefinitions("Auto,8,*,8,Auto"),
        };
        content.Children.Add(form);
        Grid.SetRow(errorText, 2);
        content.Children.Add(errorText);
        Grid.SetRow(actions, 4);
        content.Children.Add(actions);
        Content = content;

        Opened += (_, _) =>
        {
            nameBox.Focus();
            nameBox.SelectAll();
        };
        KeyDown += onKeyDown;
    }

    public static Task<GameVariableCreation?> ShowAsync(
        Window owner,
        IEnumerable<string> existingNames,
        IEnumerable<string> typeNames)
    {
        AddGameVariableDialog dialog = new(existingNames, typeNames);
        return dialog.ShowDialog<GameVariableCreation?>(owner);
    }

    private static void addRow(Grid form, int row, string label, Control editor)
    {
        TextBlock text = new()
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetRow(text, row);
        form.Children.Add(text);
        Grid.SetRow(editor, row);
        Grid.SetColumn(editor, 2);
        form.Children.Add(editor);
    }

    private void confirmSelection()
    {
        string name = nameBox.Text?.Trim() ?? string.Empty;
        errorText.Text = string.Empty;
        if (name.Length == 0)
        {
            errorText.Text = LocaleService.Get("ADD_EMPTY");
            return;
        }
        if (existingNames.Contains(name))
        {
            errorText.Text = LocaleService.Get("GAME_VARIABLE_EXISTS");
            return;
        }
        if (typeBox.SelectedItem is not string typeName)
        {
            errorText.Text = LocaleService.Get("GAME_VARIABLE_TYPE_REQUIRED");
            return;
        }
        Close(new GameVariableCreation(name, typeName));
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close(null);
            args.Handled = true;
        }
        else if (args.Key == Key.Enter)
        {
            confirmSelection();
            args.Handled = true;
        }
    }
}
