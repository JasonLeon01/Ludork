using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Diagnostics;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Controls;

public sealed partial class ParticleEditor
{
    private readonly ParticlePreviewSurface previewSurface = new();
    private readonly TextBlock previewStatus = new() { TextWrapping = TextWrapping.Wrap, Foreground = EditorTheme.Brush("TextMuted"), Margin = new Thickness(12) };
    private readonly TextBlock statistics = new() { Foreground = EditorTheme.Brush("TextMuted"), Margin = new Thickness(6), TextWrapping = TextWrapping.Wrap };
    private readonly Button playButton = new() { Content = LocaleService.Get("PARTICLE_PLAY") };
    private readonly Slider timeline = new() { Minimum = 0, Maximum = 10 };
    private readonly NumericUpDown seekTime = EditorInputs.CreateNumericUpDown(0, 0, 3600, 0.1m);
    private readonly NumericUpDown playbackSpeed = EditorInputs.CreateNumericUpDown(1, 0.01m, 16, 0.1m);
    private readonly NumericUpDown previewZoom = EditorInputs.CreateNumericUpDown(1, 0.05m, 32, 0.1m);
    private readonly Stopwatch playbackClock = new();
    private readonly DispatcherTimer playbackTimer = new() { Interval = TimeSpan.FromMilliseconds(16) };
    private ParticlePreviewClient? previewClient;
    private CancellationTokenSource? previewLifetime;
    private Task previewWorker = Task.CompletedTask;
    private bool workerRunning;
    private bool pendingLoad;
    private bool pendingSeek;
    private bool pendingRender;
    private bool playing;
    private bool syncingTime;
    private double currentTime;
    private double targetTime;
    private double pendingDelta;
    private double lastClockTime;
    private long generation;

    private Control createPreview()
    {
        Grid root = new();
        root.Children.Add(previewSurface);
        root.Children.Add(previewStatus);
        previewSurface.SizeChanged += (_, _) => requestRender();
        return root;
    }

    private Control createPlaybackToolbar()
    {
        StackPanel panel = new() { Spacing = 4 };
        WrapPanel actions = new() { Orientation = Orientation.Horizontal, Margin = new Thickness(6) };
        playButton.Click += (_, _) => setPlaying(!playing);
        actions.Children.Add(playButton);
        actions.Children.Add(button("PARTICLE_STOP", () => { setPlaying(false); requestSeek(0); }));
        actions.Children.Add(button("PARTICLE_RESTART", () => { requestSeek(0); setPlaying(true); }));
        actions.Children.Add(button("PARTICLE_STEP", () =>
        {
            setPlaying(false);
            requestSeek(currentTime + 1 / number(data["simulationRate"], 60));
        }));
        seekTime.Width = 90;
        playbackSpeed.Width = 82;
        previewZoom.Width = 82;
        actions.Children.Add(new TextBlock { Text = "s", VerticalAlignment = VerticalAlignment.Center });
        actions.Children.Add(seekTime);
        actions.Children.Add(new TextBlock { Text = "×", VerticalAlignment = VerticalAlignment.Center });
        actions.Children.Add(playbackSpeed);
        actions.Children.Add(new TextBlock { Text = LocaleService.Get("PARTICLE_ZOOM"), VerticalAlignment = VerticalAlignment.Center });
        actions.Children.Add(previewZoom);
        foreach (Control child in actions.Children)
            child.Margin = new Thickness(0, 0, 5, 4);
        seekTime.ValueChanged += (_, _) =>
        {
            if (!syncingTime && seekTime.Value is decimal value)
            {
                setPlaying(false);
                requestSeek((double)value);
            }
        };
        timeline.ValueChanged += (_, _) =>
        {
            if (!syncingTime)
            {
                setPlaying(false);
                requestSeek(timeline.Value);
            }
        };
        previewZoom.ValueChanged += (_, _) => requestRender();
        playbackSpeed.ValueChanged += (_, _) => requestRender();
        playbackTimer.Tick += (_, _) =>
        {
            double now = playbackClock.Elapsed.TotalSeconds;
            if (playing)
            {
                pendingDelta += Math.Min(0.25, Math.Max(0, now - lastClockTime));
                schedulePreview();
            }
            lastClockTime = now;
        };
        panel.Children.Add(actions);
        panel.Children.Add(timeline);
        panel.Children.Add(statistics);
        return panel;
    }

    private void initializePlayback()
    {
        previewLifetime = new CancellationTokenSource();
        previewClient = new ParticlePreviewClient(runtime);
        previewStatus.Text = LocaleService.Get("LOADING");
        previewStatus.IsVisible = true;
        runtime.Changed += onPreviewRuntimeChanged;
        playbackClock.Restart();
        lastClockTime = 0;
        requestLoad();
    }

    private void onPreviewRuntimeChanged(object? sender, EventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onPreviewRuntimeChanged(sender, args));
            return;
        }
        if (previewClient is null)
            return;
        if (runtime.IsReady)
            requestLoad();
        else
        {
            setPlaying(false);
            previewSurface.Clear();
            previewStatus.Text = runtime.StatusMessage;
            previewStatus.IsVisible = true;
        }
    }

    private void setPlaying(bool value)
    {
        playing = value;
        playButton.Content = LocaleService.Get(value ? "PARTICLE_PAUSE" : "PARTICLE_PLAY");
        pendingDelta = 0;
        lastClockTime = playbackClock.Elapsed.TotalSeconds;
        if (value)
            playbackTimer.Start();
        else
            playbackTimer.Stop();
    }

    private void requestLoad()
    {
        if (previewClient is null)
            return;
        generation++;
        pendingLoad = true;
        pendingSeek = false;
        pendingDelta = 0;
        targetTime = currentTime;
        schedulePreview();
    }

    private void requestSeek(double time)
    {
        generation++;
        pendingSeek = true;
        pendingDelta = 0;
        targetTime = time;
        schedulePreview();
    }

    private void requestRender()
    {
        pendingRender = true;
        schedulePreview();
    }

    private void schedulePreview()
    {
        if (!workerRunning && previewClient is not null && previewLifetime is { IsCancellationRequested: false })
            previewWorker = runPreviewAsync(previewClient, previewLifetime.Token);
    }

    private JsonObject previewAsset()
    {
        JsonObject asset = (JsonObject)data.DeepClone();
        JsonArray previewTracks = asset["tracks"] as JsonArray ?? new JsonArray();
        for (int index = 0; index < previewTracks.Count; index++)
        {
            if (previewTracks[index] is not JsonObject item)
                continue;
            if (solo.IsChecked == true && index != selectedTrack)
                item["enabled"] = false;
            if (item["curves"] is not JsonObject curves)
                continue;
            foreach (string channel in curves.Select(pair => pair.Key).ToArray())
            {
                if (curves[channel] is JsonValue value && value.TryGetValue<string>(out string? key)
                    && gameData.CurvesData.TryGetValue(key, out JsonObject? curve))
                    curves[channel] = curve.DeepClone();
            }
        }
        return asset;
    }

    private async Task runPreviewAsync(ParticlePreviewClient client, CancellationToken token)
    {
        workerRunning = true;
        try
        {
            while (!token.IsCancellationRequested && (pendingLoad || pendingSeek || pendingRender || pendingDelta > 0))
            {
                long revision = generation;
                string command = pendingLoad ? "load" : pendingSeek ? "seek" : pendingDelta > 0 ? "advance" : "render";
                JsonObject? asset = command == "load" ? previewAsset() : null;
                double delta = pendingDelta;
                double target = targetTime;
                pendingRender = false;
                if (command == "load")
                    pendingLoad = false;
                if (command == "advance")
                    pendingDelta = 0;
                int width = Math.Clamp((int)previewSurface.Bounds.Width, 360, 1920);
                int height = Math.Clamp((int)previewSurface.Bounds.Height, 240, 1080);
                ParticlePreviewFrame? frame = await client.RenderAsync(command, revision, width, height,
                    (double)(previewZoom.Value ?? 1), (double)(playbackSpeed.Value ?? 1), Key, asset,
                    target, delta, token);
                if (revision != generation || frame is null)
                    continue;
                if (command == "load" && target > 0)
                    pendingSeek = true;
                if (command == "seek")
                    pendingSeek = frame.Seeking;
                currentTime = frame.Time;
                previewSurface.SetFrame(frame);
                previewStatus.Text = frame.Seeking ? LocaleService.Get("PARTICLE_SEEKING") : string.Empty;
                previewStatus.IsVisible = frame.Seeking;
                syncingTime = true;
                timeline.Maximum = Math.Max(10, Math.Ceiling(Math.Max(currentTime, targetTime)));
                timeline.Value = Math.Min(currentTime, timeline.Maximum);
                seekTime.Value = (decimal)Math.Min(currentTime, 3600);
                syncingTime = false;
                string alive = frame.AliveCount?.ToString() ?? "—";
                string sampledTime = frame.SampledTime?.ToString("0.00") ?? "—";
                string simulation = frame.GpuSimulationMilliseconds?.ToString("0.00") ?? "—";
                string draw = frame.GpuDrawMilliseconds?.ToString("0.00") ?? "—";
                statistics.Text = LocaleService.Get("PARTICLE_STATS")
                    .Replace("{alive}", alive, StringComparison.Ordinal).Replace("{capacity}", frame.Capacity.ToString(), StringComparison.Ordinal)
                    .Replace("{sampledTime}", sampledTime, StringComparison.Ordinal)
                    .Replace("{simulation}", simulation, StringComparison.Ordinal).Replace("{draw}", draw, StringComparison.Ordinal)
                    .Replace("{readback}", frame.ReadbackMilliseconds.ToString("0.00"), StringComparison.Ordinal)
                    .Replace("{transfer}", frame.TransferMilliseconds.ToString("0.00"), StringComparison.Ordinal);
                if (pendingSeek)
                    await Task.Yield();
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested)
        {
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception) || exception is InvalidOperationException)
        {
            if (!token.IsCancellationRequested)
            {
                pendingLoad = pendingSeek = pendingRender = false;
                setPlaying(false);
                previewSurface.Clear();
                previewStatus.Text = exception.Message;
                previewStatus.IsVisible = true;
            }
        }
        finally
        {
            workerRunning = false;
        }
    }

    private async void disposePlayback()
    {
        runtime.Changed -= onPreviewRuntimeChanged;
        setPlaying(false);
        CancellationTokenSource? lifetime = previewLifetime;
        ParticlePreviewClient? client = previewClient;
        previewLifetime = null;
        previewClient = null;
        lifetime?.Cancel();
        if (client is not null)
            await client.DisposeAsync();
        await previewWorker;
        lifetime?.Dispose();
    }
}
