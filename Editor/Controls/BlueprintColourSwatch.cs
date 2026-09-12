using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using System;

namespace Ludork.Controls;

internal sealed class BlueprintColourSwatch : Button
{
    private readonly Border fill = new()
    {
        Margin = new Thickness(1),
        HorizontalAlignment = HorizontalAlignment.Stretch,
        VerticalAlignment = VerticalAlignment.Stretch,
    };
    private Color colour;

    public BlueprintColourSwatch(Color initial)
    {
        Width = 54;
        Height = 28;
        MinWidth = 54;
        Padding = new Thickness(0);
        BorderBrush = Ludork.Services.EditorTheme.Brush("Border");
        BorderThickness = new Thickness(1);
        Grid content = new()
        {
            Width = 50,
            Height = 24,
        };
        content.Children.Add(new BlueprintCheckerboard
        {
            CellSize = 5,
            Margin = new Thickness(1),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch,
        });
        content.Children.Add(fill);
        Content = content;
        Colour = initial;
    }

    public Color Colour
    {
        get => colour;
        set
        {
            colour = value;
            fill.Background = new SolidColorBrush(value);
        }
    }
}

internal sealed class BlueprintCheckerboard : Control
{
    public static readonly StyledProperty<int> CellSizeProperty =
        AvaloniaProperty.Register<BlueprintCheckerboard, int>(nameof(CellSize), 5);

    public int CellSize
    {
        get => GetValue(CellSizeProperty);
        set => SetValue(CellSizeProperty, value);
    }

    public override void Render(DrawingContext context)
    {
        int size = Math.Max(1, CellSize);
        int columns = Math.Max(1, (int)Math.Ceiling(Bounds.Width / size));
        int rows = Math.Max(1, (int)Math.Ceiling(Bounds.Height / size));
        IBrush light = new SolidColorBrush(Color.Parse("#d8d8d8"));
        IBrush dark = new SolidColorBrush(Color.Parse("#9c9c9c"));
        for (int row = 0; row < rows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                context.FillRectangle(
                    (row + column) % 2 == 0 ? light : dark,
                    new Rect(column * size, row * size, size, size));
            }
        }
    }
}
