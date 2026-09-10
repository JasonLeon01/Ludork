using System;

namespace Ludork.Views;

public sealed class ReferenceNodeOpenEventArgs(string nodeId) : EventArgs
{
    public string NodeId { get; } = nodeId;
}
