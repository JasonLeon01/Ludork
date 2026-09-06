using System;

namespace Ludork.Controls;

public sealed class UiPreviewTransformEventArgs : EventArgs
{
    public UiPreviewTransformEventArgs(
        string nodeName,
        double deltaX,
        double deltaY,
        bool resize)
    {
        NodeName = nodeName;
        DeltaX = deltaX;
        DeltaY = deltaY;
        Resize = resize;
    }

    public string NodeName { get; }
    public double DeltaX { get; }
    public double DeltaY { get; }
    public bool Resize { get; }
}
