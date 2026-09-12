using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using Ludork.Views.Utils;

namespace Ludork.Views;

public sealed class PerformanceMonitorWindow : Window
{
    private readonly PerformanceMonitorCanvas canvas = new();
    private double? lastMainFrameTime;

    public PerformanceMonitorWindow()
    {
        Title = LocaleService.Get("PERFORMANCE_MONITOR");
        Width = 800;
        Height = 400;
        MinWidth = 720;
        MinHeight = 340;
        Background = Ludork.Services.EditorTheme.Brush("Background");
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        EditorWindowIcon.Apply(this);
        Content = canvas;
    }

    public void ClearData()
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(ClearData);
            return;
        }
        canvas.ClearData();
        lastMainFrameTime = null;
    }

    public void AddSample(PerformanceSample sample)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => AddSample(sample));
            return;
        }
        if (sample.ProtocolVersion != PerformanceSample.CurrentProtocolVersion || sample.Fps <= 0 || !double.IsFinite(sample.Fps))
            return;
        if (lastMainFrameTime is double previousTime
            && sample.MainFrames.Count != 0
            && sample.MainFrames[0].Time < previousTime)
        {
            canvas.ClearData();
        }
        if (sample.MainFrames.Count != 0)
            lastMainFrameTime = sample.MainFrames[^1].Time;
        canvas.AddSample(sample);
    }

    public void AddSample(double fps, double memoryMegabytes)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => AddSample(fps, memoryMegabytes));
            return;
        }
        if (!double.IsFinite(fps) || fps <= 0)
            return;
        double active = 1000.0 / fps;
        double time = lastMainFrameTime is double previousTime
            ? previousTime + active / 1000.0
            : 0.0;
        MainFrameTiming frame = new(
            time,
            active,
            active,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0,
            0.0);
        AddSample(new PerformanceSample(fps, memoryMegabytes)
        {
            SampleFrames = 1,
            MainFrames = new[] { frame },
        });
    }
}
