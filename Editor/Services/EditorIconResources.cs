using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using System;

namespace Ludork.Services;

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

}
