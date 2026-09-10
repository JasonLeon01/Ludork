using System;

namespace Ludork.Services;

internal sealed class EditorDocumentNotificationBatch(EditorDocumentRegistry registry) : IDisposable
{
    private bool completed;

    public void Commit()
    {
        if (completed)
            return;
        registry.ValidateNotificationScope(this);
        completed = true;
        registry.CompleteNotificationScope(this, true);
    }

    public void Dispose()
    {
        if (completed)
            return;
        registry.ValidateNotificationScope(this);
        completed = true;
        registry.CompleteNotificationScope(this, false);
    }
}
