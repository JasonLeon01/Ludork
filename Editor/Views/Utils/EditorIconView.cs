using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Services;

namespace Ludork.Views.Utils;

internal static class EditorIconView
{
    public static Image CreateImage(string resourceKey, double width, double height)
    {
        return new Image
        {
            Source = EditorIconResources.GetImage(resourceKey),
            Width = width,
            Height = height,
            Stretch = Stretch.Uniform,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
        };
    }
}
