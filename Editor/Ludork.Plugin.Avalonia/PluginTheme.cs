using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using System;

namespace Ludork.Plugin.Avalonia;

public static class PluginTheme
{
    public static IBrush Brush(string key) => Resource<IBrush>($"Editor.{key}Brush");

    public static Color Color(string key) => Resource<Color>($"Editor.{key}Color");

    public static FontFamily FontFamily => Resource<FontFamily>("Editor.FontFamily");

    private static T Resource<T>(string key)
    {
        Application? application = Application.Current;
        if (application is not null
            && application.TryGetResource(key, application.ActualThemeVariant, out object? value)
            && value is T resource)
            return resource;
        throw new InvalidOperationException($"Editor theme resource '{key}' is unavailable.");
    }
}
