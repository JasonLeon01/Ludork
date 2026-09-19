using Avalonia.Threading;
using Ludork.Plugin.Abstractions;
using System;
using System.Threading;

namespace Ludork.Plugins.OfficialResourceCleanup.UI;

internal sealed class ResourceCleanupProgressSink : IProgress<ResourceCleanupProgress>, IDisposable
{
    private readonly DispatcherTimer timer;
    private ResourceCleanupProgress? latest;

    public ResourceCleanupProgressSink(Action<ResourceCleanupProgress> update)
    {
        timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        timer.Tick += (_, _) =>
        {
            ResourceCleanupProgress? value = Interlocked.Exchange(ref latest, null);
            if (value is not null)
                update(value);
        };
        timer.Start();
    }

    public void Report(ResourceCleanupProgress value) => Interlocked.Exchange(ref latest, value);

    public void Dispose() => timer.Stop();
}
