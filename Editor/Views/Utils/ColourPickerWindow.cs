using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Services;
using System;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public sealed class ColourPickerWindow : Window
{
    private readonly LudorkColourPicker colourPicker;
    private Color? result;

    private ColourPickerWindow(Color initial)
    {
        Title = LocaleService.Get("COLOUR_PICKER_TITLE");
        Width = 820;
        Height = 500;
        MinWidth = 700;
        MinHeight = 460;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);

        colourPicker = new LudorkColourPicker(initial);
        colourPicker.ScreenPickRequested += onScreenPickRequested;

        Button confirm = new() { Content = LocaleService.Get("CONFIRM") };
        confirm.Click += (_, _) => confirmSelection();
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close();

        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        Grid content = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("*,Auto"),
            RowSpacing = 10,
        };
        content.Children.Add(colourPicker);
        Grid.SetRow(actions, 1);
        content.Children.Add(actions);
        Content = content;
        KeyDown += onKeyDown;
    }

    public static async Task<Color?> ShowAsync(Window owner, Color initial)
    {
        ColourPickerWindow window = new(initial);
        await window.ShowDialog(owner);
        return window.result;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close();
            args.Handled = true;
            return;
        }
        if (args.Key is Key.Enter or Key.Return)
        {
            confirmSelection();
            args.Handled = true;
        }
    }

    private void confirmSelection()
    {
        result = colourPicker.Color;
        Close();
    }

    private async void onScreenPickRequested(object? sender, EventArgs args)
    {
        Color? sampled = await ScreenColourOverlay.ShowAsync(this);
        if (sampled is Color color)
            colourPicker.SetScreenColour(color);
    }
}
