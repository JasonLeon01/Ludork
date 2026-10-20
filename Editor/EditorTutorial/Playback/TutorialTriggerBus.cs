using System;

namespace Ludork.EditorTutorial.Playback;

public sealed class TutorialTriggerBus : IDisposable
{
    private bool disposed;

    public event EventHandler<string>? Triggered;

    public void Publish(string triggerKey)
    {
        if (!disposed && !string.IsNullOrWhiteSpace(triggerKey))
            Triggered?.Invoke(this, triggerKey);
    }

    public void Dispose()
    {
        disposed = true;
        Triggered = null;
    }
}
