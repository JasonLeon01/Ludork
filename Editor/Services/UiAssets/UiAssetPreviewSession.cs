using Avalonia.Threading;
using Ludork.Models;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services.UiAssets;

internal sealed class UiAssetPreviewSession : IAsyncDisposable
{
    private readonly UiAssetEditorDocument document;
    private readonly GameDataService gameData;
    private readonly UiPreviewRuntimeService runtime;
    private readonly UiPreviewClient client;
    private readonly CancellationTokenSource lifetime = new();
    private CancellationTokenSource? debounceCancellation;
    private CancellationTokenSource? renderCancellation;
    private Task workerTask = Task.CompletedTask;
    private Task? disposalTask;
    private AssetSnapshot? assetSnapshot;
    private UiPreviewAnimationSample? animationSample;
    private string? inputError;
    private double renderScale = 1;
    private long requestVersion;
    private long frameEpoch;
    private long publishedFrameEpoch;
    private long frameGeneration;
    private bool hasRequest;
    private bool pending;
    private bool immediate;
    private bool workerRunning;
    private bool disposed;

    public UiAssetPreviewSession(
        UiAssetEditorDocument document,
        GameDataService gameData,
        UiPreviewRuntimeService runtime)
    {
        Dispatcher.UIThread.VerifyAccess();
        this.document = document;
        this.gameData = gameData;
        this.runtime = runtime;
        client = new UiPreviewClient(runtime);
        client.StateChanged += onClientStateChanged;
        runtime.Changed += onRuntimeChanged;
    }

    public event EventHandler<UiPreviewFrame>? FrameReady;
    public event EventHandler? StateChanged;

    public UiPreviewClientState State => disposed || !runtime.IsReady
        ? UiPreviewClientState.Unavailable
        : inputError is null ? client.State : UiPreviewClientState.Faulted;

    public string StatusMessage => disposed
        ? string.Empty
        : runtime.IsReady ? inputError ?? client.StatusMessage : runtime.StatusMessage;

    public void RequestRefresh(
        double renderScale,
        UiPreviewAnimationSample? sample,
        bool immediate = false)
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposed)
            return;
        frameEpoch++;
        assetSnapshot = null;
        setRequest(renderScale, sample);
        schedule(immediate);
    }

    public void RequestAnimationSample(double renderScale, UiPreviewAnimationSample? sample)
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposed)
            return;
        if (this.renderScale != renderScale
            || !string.Equals(animationSample?.Name, sample?.Name, StringComparison.Ordinal)
            || !string.Equals(animationSample?.Target, sample?.Target, StringComparison.Ordinal))
        {
            frameEpoch++;
        }
        setRequest(renderScale, sample);
        schedule(true);
    }

    public async Task<string?> HitTestAsync(long generation, double x, double y)
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposed || !runtime.IsReady || inputError is not null || generation != frameGeneration
            || generation == 0 || publishedFrameEpoch != frameEpoch)
        {
            return null;
        }
        long epoch = frameEpoch;
        try
        {
            string? nodeName = await client.HitTestAsync(generation, x, y, lifetime.Token);
            return disposed || !runtime.IsReady || epoch != frameEpoch || generation != frameGeneration
                ? null
                : nodeName;
        }
        catch (OperationCanceledException) when (disposed)
        {
            return null;
        }
    }

    public ValueTask DisposeAsync()
    {
        Dispatcher.UIThread.VerifyAccess();
        if (disposalTask is not null)
            return new ValueTask(disposalTask);
        disposed = true;
        pending = false;
        frameEpoch++;
        assetSnapshot = null;
        lifetime.Cancel();
        client.StateChanged -= onClientStateChanged;
        runtime.Changed -= onRuntimeChanged;
        disposalTask = disposeAsync();
        return new ValueTask(disposalTask);
    }

    private async Task disposeAsync()
    {
        Task clientDisposal = client.DisposeAsync().AsTask();
        try
        {
            await Task.WhenAll(clientDisposal, workerTask);
        }
        finally
        {
            lifetime.Dispose();
        }
    }

    private void setRequest(double scale, UiPreviewAnimationSample? sample)
    {
        renderCancellation?.Cancel();
        if (!double.IsFinite(scale) || scale <= 0)
            throw new ArgumentOutOfRangeException(nameof(scale));
        renderScale = scale;
        animationSample = sample;
        hasRequest = true;
    }

    private void schedule(bool renderImmediately)
    {
        requestVersion++;
        pending = true;
        immediate |= renderImmediately;
        debounceCancellation?.Cancel();
        if (!workerRunning)
            workerTask = runWorkerAsync();
    }

    private async Task runWorkerAsync()
    {
        workerRunning = true;
        try
        {
            while (pending && !disposed)
            {
                long version = requestVersion;
                bool renderImmediately = immediate;
                pending = false;
                immediate = false;
                if (!renderImmediately)
                {
                    using CancellationTokenSource delayCancellation =
                        CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token);
                    debounceCancellation = delayCancellation;
                    try
                    {
                        await Task.Delay(140, delayCancellation.Token);
                    }
                    catch (OperationCanceledException) when (!disposed)
                    {
                        continue;
                    }
                    finally
                    {
                        debounceCancellation = null;
                    }
                    if (version != requestVersion)
                        continue;
                }
                await renderAsync();
            }
        }
        catch (OperationCanceledException) when (disposed)
        {
        }
        finally
        {
            workerRunning = false;
        }
    }

    private async Task renderAsync()
    {
        UiPreviewRuntimeSnapshot? snapshot = runtime.Current;
        if (disposed || !runtime.IsReady || snapshot is null)
            return;
        long version = requestVersion;
        UiPreviewAnimationSample? sample = animationSample;
        if (sample is not null && (string.IsNullOrWhiteSpace(sample.Name)
            || !double.IsFinite(sample.Time) || sample.Time < 0))
        {
            setInputError("UI preview animation sample is invalid.");
            return;
        }
        setInputError(null);
        if (disposed || version != requestVersion)
            return;
        long epoch = frameEpoch;
        AssetSnapshot source = assetSnapshot ??= captureAssetSnapshot();
        double scale = renderScale;
        using CancellationTokenSource renderLifetime = CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token);
        renderCancellation = renderLifetime;
        UiPreviewFrame? frame;
        try
        {
            frame = await client.RenderAsync(source.Key, source.Asset, source.Dependencies,
                scale, sample, renderLifetime.Token);
        }
        catch (OperationCanceledException) when (renderLifetime.IsCancellationRequested)
        {
            return;
        }
        finally
        {
            if (ReferenceEquals(renderCancellation, renderLifetime))
                renderCancellation = null;
        }
        if (frame is null || disposed || epoch != frameEpoch || !runtime.IsReady
            || snapshot.BuildId != runtime.Current?.BuildId
            || snapshot.RegistryHash != runtime.Current?.RegistryHash
            || snapshot.HostPath != runtime.Current?.HostPath)
        {
            return;
        }
        publishedFrameEpoch = epoch;
        frameGeneration = frame.Generation;
        FrameReady?.Invoke(this, frame);
    }

    private void setInputError(string? message)
    {
        if (string.Equals(inputError, message, StringComparison.Ordinal))
            return;
        inputError = message;
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private AssetSnapshot captureAssetSnapshot()
    {
        string assetKey = document.AssetKey;
        JsonObject asset = (JsonObject)document.Data.DeepClone();
        UiAssetDependencyGraph dependencyGraph = new(gameData.UiAssetsData, assetKey, asset);
        return new AssetSnapshot(assetKey, asset, dependencyGraph.CollectDependencies(assetKey));
    }

    private void onClientStateChanged(object? sender, EventArgs args)
    {
        if (disposed)
            return;
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onClientStateChanged(sender, args));
            return;
        }
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private void onRuntimeChanged(object? sender, EventArgs args)
    {
        if (disposed)
            return;
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onRuntimeChanged(sender, args));
            return;
        }
        frameEpoch++;
        assetSnapshot = null;
        if (!runtime.IsReady)
        {
            pending = false;
            immediate = false;
            debounceCancellation?.Cancel();
        }
        else if (hasRequest)
        {
            schedule(false);
        }
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private sealed record AssetSnapshot(
        string Key,
        JsonObject Asset,
        IReadOnlyDictionary<string, JsonObject> Dependencies);
}
