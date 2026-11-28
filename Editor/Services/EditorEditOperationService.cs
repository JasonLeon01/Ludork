using System;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed class EditorEditOperationService
{
    public event EventHandler? Changed;
    private TaskCompletionSource? operation;
    public bool IsBusy => operation is not null;
    internal Task Completion => operation?.Task ?? Task.CompletedTask;

    public bool TryBegin()
    {
        if (IsBusy)
            return false;
        operation = new(TaskCreationOptions.RunContinuationsAsynchronously);
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void Complete()
    {
        TaskCompletionSource? completed = operation;
        operation = null;
        completed?.TrySetResult();
        Changed?.Invoke(this, EventArgs.Empty);
    }
}
