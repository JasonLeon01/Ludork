using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.ResourceCleanup;

internal static class SystemTrashService
{
    public static Task<T> RunAsync<T>(Func<ISystemTrash, T> operation, CancellationToken cancellationToken)
    {
        if (OperatingSystem.IsMacOS())
        {
            ISystemTrash macTrash = new MacOSTrashService();
            return Task.Run(() => operation(macTrash), cancellationToken);
        }
        if (!OperatingSystem.IsWindows())
            throw new PlatformNotSupportedException("System Trash is supported on Windows and macOS.");
        ISystemTrash windowsTrash = new WindowsTrashService();
        TaskCompletionSource<T> completion = new(TaskCreationOptions.RunContinuationsAsynchronously);
        Thread thread = new(() =>
        {
            try
            {
                cancellationToken.ThrowIfCancellationRequested();
                completion.SetResult(operation(windowsTrash));
            }
            catch (OperationCanceledException)
            {
                completion.SetCanceled(cancellationToken);
            }
            catch (Exception exception)
            {
                completion.SetException(exception);
            }
        }) { IsBackground = true, Name = "Ludork resource recycling" };
        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        return completion.Task;
    }
}
