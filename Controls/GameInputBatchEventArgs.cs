using Ludork.Services;
using System;
using System.Collections.Generic;

namespace Ludork.Controls;

public sealed class GameInputBatchEventArgs(IReadOnlyList<RuntimeInputEvent> events) : EventArgs
{
    public IReadOnlyList<RuntimeInputEvent> Events { get; } = events;
}
