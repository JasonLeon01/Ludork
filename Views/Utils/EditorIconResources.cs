using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using System;

namespace Ludork.Views.Utils;

internal static class EditorIconResources
{
    public static Geometry GetGeometry(string resourceKey)
    {
        if (Application.Current?.FindResource(resourceKey) is not Geometry geometry)
            throw new InvalidOperationException($"Editor icon geometry resource '{resourceKey}' is unavailable.");
        return geometry;
    }

    public static IImage GetImage(string resourceKey)
    {
        if (Application.Current?.FindResource(resourceKey) is not IImage image)
            throw new InvalidOperationException($"Editor icon image resource '{resourceKey}' is unavailable.");
        return image;
    }

    public static Image CreateImage(string resourceKey, double width, double height)
    {
        return new Image
        {
            Source = GetImage(resourceKey),
            Width = width,
            Height = height,
            Stretch = Stretch.Uniform,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
        };
    }
}
