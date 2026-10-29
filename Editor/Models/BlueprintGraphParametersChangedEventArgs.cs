using System;
using System.Collections.Generic;

namespace Ludork.Models;

internal sealed class BlueprintGraphParametersChangedEventArgs(string eventName, IReadOnlyList<int> nodeIndices) : EventArgs
{
    public string EventName { get; } = eventName;
    public IReadOnlyList<int> NodeIndices { get; } = nodeIndices;
}
