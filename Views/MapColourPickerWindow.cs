using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Threading.Tasks;

namespace Ludork.Views;

internal sealed class MapColourPickerWindow : Window
{
    private readonly LudorkColourPicker colourPicker;

    private MapColourPickerWindow(Color initial)
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
        confirm.Click += (_, _) => Close(colourPicker.Color);
        Button cancel = new() { Content = LocaleService.Get("CANCEL") };
        cancel.Click += (_, _) => Close(null);
        Grid content = new() { Margin = new Thickness(12), RowDefinitions = new RowDefinitions("*,Auto"), RowSpacing = 10 };
        content.Children.Add(colourPicker);
        StackPanel actions = new()
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Spacing = 8,
            Children = { confirm, cancel },
        };
        Grid.SetRow(actions, 1);
        content.Children.Add(actions);
        Content = content;
        KeyDown += onKeyDown;
    }

    public static Task<Color?> ShowAsync(Window owner, Color initial)
    {
        return new MapColourPickerWindow(initial).ShowDialog<Color?>(owner);
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key == Key.Escape)
        {
            Close(null);
            args.Handled = true;
            return;
        }
        if (args.Key is Key.Enter or Key.Return)
        {
            Close(colourPicker.Color);
            args.Handled = true;
        }
    }

    private async void onScreenPickRequested(object? sender, EventArgs args)
    {
        Color? result = await ScreenColourOverlay.ShowAsync(this);
        if (result is Color color)
            colourPicker.SetScreenColour(color);
    }
}
