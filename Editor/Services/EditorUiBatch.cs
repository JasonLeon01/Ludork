using Avalonia.Threading;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class EditorUiBatch
{
    private readonly Stopwatch clock = Stopwatch.StartNew();

    public async ValueTask YieldIfNeededAsync(CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (clock.Elapsed.TotalMilliseconds < 8)
            return;
        await YieldAsync(cancellationToken);
        clock.Restart();
    }

    public static async Task YieldAsync(CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        await Dispatcher.UIThread.InvokeAsync(static () => { }, DispatcherPriority.Background);
        cancellationToken.ThrowIfCancellationRequested();
    }
}
