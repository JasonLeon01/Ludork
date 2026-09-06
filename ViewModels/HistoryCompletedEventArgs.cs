using System.Collections.Generic;
using System;

namespace Ludork.ViewModels;

public sealed class HistoryCompletedEventArgs(string action, IReadOnlyList<string> differences) : EventArgs
{
    public string Action { get; } = action;
    public IReadOnlyList<string> Differences { get; } = differences;
}
