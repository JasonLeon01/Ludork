using System;
using System.Collections.Generic;

namespace Ludork.Services;

public sealed class ExternalFilesSynchronizedEventArgs(IReadOnlyList<string> paths) : EventArgs
{
    public IReadOnlyList<string> Paths { get; } = paths;
}
