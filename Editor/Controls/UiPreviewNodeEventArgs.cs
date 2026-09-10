using System;

namespace Ludork.Controls;

public sealed class UiPreviewNodeEventArgs : EventArgs
{
    public UiPreviewNodeEventArgs(string nodeName)
    {
        NodeName = nodeName;
    }

    public string NodeName { get; }
}
