using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Primitives.PopupPositioning;
using Avalonia.Media;
using Avalonia.Threading;
using System;

namespace Ludork.Controls;

public sealed class Toast
{
    private readonly TextBlock message = new()
    {
        FontWeight = FontWeight.Bold,
        TextWrapping = TextWrapping.Wrap,
        MaxWidth = 460,
    };
    private readonly Popup popup;
    private readonly DispatcherTimer timer = new();

    public Toast(Control owner)
    {
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
                Background = new SolidColorBrush(Color.FromArgb(220, 50, 50, 50)),
                CornerRadius = new CornerRadius(5),
                Padding = new Thickness(10),
                Child = message,
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
