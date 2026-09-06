using System;

namespace Ludork.Views;

public sealed class BlueprintGraphOrganizeRequestedEventArgs(string eventName) : EventArgs
{
    public string EventName { get; } = eventName;
}
