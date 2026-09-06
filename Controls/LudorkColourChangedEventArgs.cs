using Avalonia.Media;
using System;

namespace Ludork.Controls;

public sealed class LudorkColourChangedEventArgs(Color color) : EventArgs
{
    public Color Color { get; } = color;
}
