using Ludork.Services;
using System;

namespace Ludork.ViewModels;

public sealed class HistoryCompletedEventArgs(string action, HistoryResult result) : EventArgs
{
    public string Action { get; } = action;
    public HistoryResult Result { get; } = result;
}
