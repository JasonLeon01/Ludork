using System;

namespace Ludork.Services;

public sealed class EditorEditOperationService
{
    public event EventHandler? Changed;
    public bool IsBusy { get; private set; }

    public bool TryBegin()
    {
        if (IsBusy)
            return false;
        IsBusy = true;
        Changed?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void Complete()
    {
        IsBusy = false;
        Changed?.Invoke(this, EventArgs.Empty);
    }
}
