using System;

namespace Ludork.Services;

internal interface IEditorNotificationBackend : IDisposable
{
    event EventHandler<EditorNotificationCompletion>? Completed;
    void Show(uint id, string title, string message);
    void Close(uint id);
}
