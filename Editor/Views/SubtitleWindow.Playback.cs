using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views;

public sealed partial class SubtitleWindow
{
    private readonly PreviewFrameSurface preview = new();
    private readonly TextBlock previewStatus = new() { Margin = new Thickness(12), TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock videoLabel = new() { TextTrimming = TextTrimming.CharacterEllipsis, MaxWidth = 230, VerticalAlignment = VerticalAlignment.Center };
    private readonly Button play = new() { Content = L("SUBTITLE_PLAY") };
    private readonly CheckBox mute = new() { Content = L("SUBTITLE_MUTE"), VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBox previewLanguage = EditorInputs.CreateEditableTextBox("en_GB");
    private readonly NumericUpDown position = EditorInputs.CreateNumericUpDown(0, 0, decimal.MaxValue, 0.1m);
    private readonly DispatcherTimer previewTimer = new() { Interval = TimeSpan.FromMilliseconds(33) };
    private SubtitlePreviewClient? client;
    private CancellationTokenSource? previewLifetime;
    private Task previewWorker = Task.CompletedTask;
    private bool workerRunning;
    private bool previewLoaded;
    private bool pendingLoad;
    private bool pendingSeek;
    private bool pendingState;
    private bool pendingRender;
    private bool assetDirty = true;
    private bool desiredPlaying;
    private bool syncingPosition;
    private bool scrubbing;
    private bool resumeAfterScrub;
    private long generation;
    private double currentTime;
    private double seekTarget;
    private double videoDuration;
    private string videoPath = string.Empty;

    private Control createPreview()
    {
        Grid result = new();
        result.Children.Add(preview);
        result.Children.Add(previewStatus);
        preview.SizeChanged += (_, _) => requestPreview("render");
        return result;
    }

    private Control createPlaybackToolbar()
    {
        WrapPanel toolbar = new() { Margin = new Thickness(8) };
        play.Click += (_, _) => requestPreview(desiredPlaying ? "pause" : "play");
        toolbar.Children.Add(play);
        toolbar.Children.Add(button("SUBTITLE_STOP", () => requestPreview("stop")));
        position.Width = 105;
        position.FormatString = "0.###";
        position.ValueChanged += (_, _) =>
        {
            if (syncingPosition || position.Value is not decimal time)
                return;
            seekTarget = (double)time;
            requestPreview("seek");
        };
        toolbar.Children.Add(position);
        toolbar.Children.Add(new TextBlock { Text = "s", VerticalAlignment = VerticalAlignment.Center });
        toolbar.Children.Add(button("SUBTITLE_SELECT_VIDEO", () => _ = selectVideoAsync()));
        toolbar.Children.Add(button("SUBTITLE_CLEAR_VIDEO", () =>
        {
            videoPath = string.Empty;
            videoLabel.Text = string.Empty;
            videoDuration = 0;
            requestPreview("load");
        }));
        toolbar.Children.Add(videoLabel);
        mute.IsCheckedChanged += (_, _) => requestPreview("render");
        toolbar.Children.Add(mute);
        toolbar.Children.Add(new TextBlock { Text = L("SUBTITLE_PREVIEW_LANGUAGE"), VerticalAlignment = VerticalAlignment.Center });
        previewLanguage.Width = 100;
        previewLanguage.TextChanged += (_, _) => requestPreview("render");
        toolbar.Children.Add(previewLanguage);
        toolbar.Children.Add(new TextBlock { Text = L("SUBTITLE_ZOOM"), VerticalAlignment = VerticalAlignment.Center });
        NumericUpDown zoom = EditorInputs.CreateNumericUpDown(1, 0.05m, 20, 0.25m);
        zoom.Width = 90;
        zoom.ValueChanged += (_, _) => timeline.SetZoom((double)(zoom.Value ?? 1));
        toolbar.Children.Add(zoom);
        foreach (Control child in toolbar.Children)
            child.Margin = new Thickness(0, 0, 6, 4);
        return toolbar;
    }

    private async Task selectVideoAsync()
    {
        string projectFile = Path.Combine(gameData.ProjectPath, "Main.proj");
        JsonObject? config = File.Exists(projectFile) ? JsonNode.Parse(await File.ReadAllTextAsync(projectFile)) as JsonObject : null;
        if (config?["ffmpeg"] is not JsonValue value || !value.TryGetValue(out bool enabled) || !enabled)
        {
            await AlertDialog.ShowAsync(this, L("SUBTITLE_EDITOR"), L("SUBTITLE_NOT_FFMPEG"));
            return;
        }
        IStorageFolder? folder = await StorageProvider.TryGetFolderFromPathAsync(Path.Combine(gameData.ProjectPath, "Assets", "Videos"));
        IReadOnlyList<IStorageFile> files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
        {
            Title = L("SUBTITLE_SELECT_VIDEO"), SuggestedStartLocation = folder, AllowMultiple = false,
            FileTypeFilter = [new FilePickerFileType(L("SUBTITLE_VIDEO_FILES")) { Patterns = ["*.mp4", "*.mov"] }],
        });
        if (files.Count == 0)
            return;
        if (!GameAssetPath.TryFromProjectFile(gameData.ProjectPath, files[0].Path.LocalPath, out string path)
            || !path.StartsWith("/Game/Assets/Videos/", StringComparison.Ordinal))
        {
            await AlertDialog.ShowAsync(this, L("SUBTITLE_EDITOR"), L("SUBTITLE_VIDEO_PATH_ERROR"));
            return;
        }
        videoPath = path;
        videoLabel.Text = Path.GetFileName(path);
        requestPreview("load");
    }

    private void initializePreview()
    {
        previewLifetime = new CancellationTokenSource();
        client = new SubtitlePreviewClient(runtime);
        runtime.Changed += runtimeChanged;
        previewTimer.Tick += previewTick;
        requestPreview("load");
    }

    private void previewTick(object? sender, EventArgs args) => requestPreview("render");

    private void runtimeChanged(object? sender, EventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => runtimeChanged(sender, args));
            return;
        }
        if (runtime.IsReady)
            requestPreview("load");
        else
        {
            previewLoaded = false;
            desiredPlaying = false;
            previewTimer.Stop();
            preview.Clear();
            previewStatus.Text = runtime.StatusMessage;
        }
    }

    private void scrub(double time, bool finished)
    {
        if (!scrubbing)
        {
            resumeAfterScrub = desiredPlaying;
            scrubbing = true;
            requestPreview("pause");
        }
        seekTarget = time;
        currentTime = time;
        timeline.CurrentTime = time;
        timeline.InvalidateVisual();
        requestPreview("seek");
        if (finished)
        {
            scrubbing = false;
            requestPreview(resumeAfterScrub ? "play" : "pause");
        }
    }

    private void requestPreview(string command)
    {
        if (command == "load")
        {
            pendingLoad = true;
            pendingSeek = false;
            desiredPlaying = false;
            currentTime = 0;
            seekTarget = 0;
            assetDirty = true;
        }
        else if (command is "play" or "pause")
        {
            desiredPlaying = command == "play";
            pendingState = true;
            if (desiredPlaying && timeline.Duration > 0 && currentTime >= timeline.Duration)
            {
                seekTarget = 0;
                pendingSeek = true;
            }
        }
        else if (command == "stop")
        {
            desiredPlaying = false;
            pendingState = true;
            seekTarget = 0;
            pendingSeek = true;
        }
        else if (command == "seek")
            pendingSeek = true;
        if (command != "render" || assetDirty)
            generation++;
        pendingRender = true;
        if (client is not null && !workerRunning && previewLifetime is { IsCancellationRequested: false })
            previewWorker = runPreviewAsync(client, previewLifetime.Token);
    }

    private async Task runPreviewAsync(SubtitlePreviewClient activeClient, CancellationToken token)
    {
        workerRunning = true;
        try
        {
            while (!token.IsCancellationRequested && (pendingLoad || pendingSeek || pendingState || pendingRender))
            {
                bool valid = inputErrors.Count == 0 && SubtitleAssetSchema.Validate(data, Key).Count == 0;
                if (!valid)
                {
                    long invalidRevision = generation;
                    desiredPlaying = false;
                    pendingSeek = false;
                    pendingState = false;
                    pendingRender = false;
                    previewTimer.Stop();
                    play.Content = L("SUBTITLE_PLAY");
                    if (previewLoaded)
                        await activeClient.RenderAsync("pause", generation, 640, 480, null,
                            previewLanguage.Text ?? "en_GB", videoPath, mute.IsChecked == true, currentTime, token);
                    if (invalidRevision != generation || pendingRender)
                        continue;
                    previewStatus.Text = L("SUBTITLE_FIX_ERRORS");
                    break;
                }
                long revision = generation;
                string command = pendingLoad ? "load" : pendingState && !desiredPlaying ? "pause" : pendingSeek ? "seek" : pendingState ? "play" : "render";
                JsonObject? asset = assetDirty || command == "load" ? (JsonObject)data.DeepClone() : null;
                assetDirty = false;
                pendingRender = false;
                if (command == "load") pendingLoad = false;
                if (command == "seek") pendingSeek = false;
                if (command is "play" or "pause") pendingState = false;
                SubtitlePreviewFrame? frame = await activeClient.RenderAsync(command, revision,
                    Math.Clamp((int)preview.Bounds.Width, 360, 1920), Math.Clamp((int)preview.Bounds.Height, 240, 1080),
                    asset, previewLanguage.Text ?? "en_GB", videoPath, mute.IsChecked == true, seekTarget, token);
                if (command == "load" && frame is not null)
                    previewLoaded = true;
                if (frame is null || revision != generation)
                    continue;
                currentTime = frame.Time;
                videoDuration = frame.VideoDuration;
                if (!pendingState && !pendingSeek)
                    desiredPlaying = frame.Playing;
                play.Content = L(desiredPlaying ? "SUBTITLE_PAUSE" : "SUBTITLE_PLAY");
                if (desiredPlaying) previewTimer.Start(); else previewTimer.Stop();
                preview.SetFrame(frame.Width, frame.Height, frame.Stride, frame.Pixels);
                JsonObject? active = sections.OfType<JsonObject>().FirstOrDefault(item =>
                    number(item["startTime"]) <= currentTime && currentTime < number(item["endTime"]));
                previewStatus.Text = active?["content"] is JsonObject translations
                    && !translations.ContainsKey(previewLanguage.Text ?? "en_GB") && !translations.ContainsKey("en_GB")
                    ? L("SUBTITLE_MISSING_LANGUAGE") : string.Empty;
                syncingPosition = true;
                position.Value = decimalTime(currentTime);
                syncingPosition = false;
                timeline.CurrentTime = currentTime;
                updateValidation();
            }
        }
        catch (OperationCanceledException) when (token.IsCancellationRequested) { }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception) || exception is InvalidOperationException)
        {
            if (!token.IsCancellationRequested)
            {
                desiredPlaying = false;
                previewTimer.Stop();
                previewStatus.Text = exception.Message;
                preview.Clear();
                previewLoaded = false;
                pendingLoad = true;
                assetDirty = true;
            }
        }
        finally
        {
            workerRunning = false;
        }
    }

    private async void disposePreview()
    {
        runtime.Changed -= runtimeChanged;
        previewTimer.Stop();
        previewTimer.Tick -= previewTick;
        CancellationTokenSource? lifetime = previewLifetime;
        SubtitlePreviewClient? activeClient = client;
        client = null;
        previewLifetime = null;
        lifetime?.Cancel();
        if (activeClient is not null)
            await activeClient.DisposeAsync();
        await previewWorker;
        lifetime?.Dispose();
        preview.Clear();
    }
}
