using Avalonia.Controls;
using Ludork.Plugin.Abstractions;

namespace Ludork.Plugin.Avalonia;

public interface IAvaloniaPluginUserInterface : IPluginUserInterface
{
    Window Owner { get; }
}
