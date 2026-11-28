using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Primitives.PopupPositioning;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using System;

namespace Ludork.Controls;

public sealed class Toast
{
    private readonly TextBlock message = new()
    {
        FontWeight = FontWeight.SemiBold,
        TextWrapping = TextWrapping.Wrap,
        MaxWidth = 460,
        VerticalAlignment = VerticalAlignment.Center,
    };
    private readonly Popup popup;
    private readonly DispatcherTimer timer = new();

    public Toast(Control owner)
    {
        message.Foreground = EditorTheme.Brush("Text");
        Border accent = new()
        {
            Width = 3,
            CornerRadius = new CornerRadius(1.5),
            Background = EditorTheme.Brush("Accent"),
            VerticalAlignment = VerticalAlignment.Stretch,
        };
        Grid content = new()
        {
            ColumnDefinitions = new ColumnDefinitions("Auto,*"),
            ColumnSpacing = 10,
        };
        content.Children.Add(accent);
        Grid.SetColumn(message, 1);
        content.Children.Add(message);
        popup = new Popup
        {
            PlacementTarget = owner,
            Placement = PlacementMode.AnchorAndGravity,
            PlacementAnchor = PopupAnchor.BottomRight,
            PlacementGravity = PopupGravity.TopLeft,
            HorizontalOffset = -20,
            VerticalOffset = -40,
            Child = new Border
            {
                Background = EditorTheme.Brush("SurfaceElevated"),
                BorderBrush = EditorTheme.Brush("Border"),
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(6),
                Padding = new Thickness(10, 8, 14, 8),
                BoxShadow = new BoxShadows(new BoxShadow
                {
                    OffsetX = 0,
                    OffsetY = 6,
                    Blur = 18,
                    Spread = 0,
                    Color = Color.FromArgb(110, 0, 0, 0),
                }),
                Child = content,
            },
        };
        timer.Tick += (_, _) => popup.IsOpen = false;
    }

    public void ShowMessage(string value, int duration = 2000)
    {
        if (string.IsNullOrWhiteSpace(value))
            return;
        message.Text = value;
        timer.Stop();
        timer.Interval = TimeSpan.FromMilliseconds(duration);
        popup.IsOpen = true;
        timer.Start();
    }
}
