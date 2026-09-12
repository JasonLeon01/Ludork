using Avalonia.Media;
using Ludork.Plugin.Avalonia;

namespace Ludork.Services;

public static class EditorTheme
{
    public static IBrush Brush(string key) => PluginTheme.Brush(key);

    public static Color Color(string key) => PluginTheme.Color(key);

    public static FontFamily FontFamily => PluginTheme.FontFamily;
}
