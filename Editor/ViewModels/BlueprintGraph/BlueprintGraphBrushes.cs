using Ludork.Services;
using Ludork.Models;
using Avalonia.Media;

namespace Ludork.ViewModels.BlueprintGraph;

internal static class BlueprintGraphBrushes
{
    public static readonly IBrush VirtualHeader = new SolidColorBrush(Color.Parse("#3c6432"));
    public static readonly IBrush ResolvedHeader = new SolidColorBrush(Color.Parse("#3b3b3b"));
    public static readonly IBrush UnresolvedHeader = new SolidColorBrush(Color.Parse("#713b3b"));
    public static readonly IBrush Execution = new SolidColorBrush(Color.Parse("#e6b84f"));
    public static readonly IBrush Parameter = new SolidColorBrush(Color.Parse("#65ad67"));
}
