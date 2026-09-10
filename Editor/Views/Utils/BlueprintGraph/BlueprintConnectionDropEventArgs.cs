using System;

namespace Ludork.Views.Utils.BlueprintGraph;

public sealed class BlueprintConnectionDropEventArgs : EventArgs
{
    public BlueprintConnectionDropEventArgs(BlueprintGraphPortViewModel source)
    {
        Source = source;
    }

    public BlueprintGraphPortViewModel Source { get; }
}
