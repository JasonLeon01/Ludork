using Ludork.Services;
using Ludork.Models;
using Avalonia.Media;

namespace Ludork.ViewModels.BlueprintGraph;

internal static class BlueprintGraphBrushes
{
    public static readonly IBrush VirtualHeader = new SolidColorBrush(Color.Parse("#2D5942"));
    public static readonly IBrush EventHeader = new SolidColorBrush(Color.Parse("#6E2C52"));
    public static readonly IBrush ParentHeader = new SolidColorBrush(Color.Parse("#6B4C2A"));
    public static readonly IBrush ResolvedHeader = new SolidColorBrush(Color.Parse("#2F3A4D"));
    public static readonly IBrush UnresolvedHeader = new SolidColorBrush(Color.Parse("#7D2B2B"));
    public static readonly IBrush Execution = new SolidColorBrush(Color.Parse("#DFE1E5"));
    public static readonly IBrush Parameter = new SolidColorBrush(Color.Parse("#5FB878"));
}
