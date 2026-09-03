using Ludork.Models;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public enum UiPreviewClientState
{
    Unavailable,
    Starting,
    Ready,
    Rendering,
    Faulted,
}

public sealed record UiPreviewNodeGeometry(
    string NodeName,
    double X,
    double Y,
    double Width,
    double Height,
    double ClipX,
    double ClipY,
    double ClipWidth,
    double ClipHeight,
    int DrawOrder,
    bool Visible);

public sealed record UiPreviewFrame(
    int DesignWidth,
    int DesignHeight,
    int Width,
    int Height,
    int Stride,
    double RenderScale,
    byte[] Pixels,
    IReadOnlyList<UiPreviewNodeGeometry> Nodes,
    long Generation);

public sealed record UiPreviewAnimationSample(
    string Name,
    string? Target,
    double Time);

public sealed class UiPreviewClient : IAsyncDisposable
{
    public const int ProtocolVersion = PreviewHostConnection.ProtocolVersion;

    private readonly PreviewHostConnection connection;
    private long generation;
    private bool disposed;
    private bool rendering;

    public UiPreviewClient(string projectPath)
    {
        connection = new PreviewHostConnection(projectPath);
        connection.StateChanged += onConnectionStateChanged;
    }

    public event EventHandler? StateChanged;

    public UiPreviewClientState State { get; private set; } = UiPreviewClientState.Unavailable;
    public string StatusMessage { get; private set; } = string.Empty;
    public bool IsReady => State == UiPreviewClientState.Ready;

    public async Task<bool> StartAsync(CancellationToken cancellationToken = default)
    {
        bool started = await connection.StartAsync(cancellationToken);
        syncConnectionState();
        if (!started)
            return false;
        if (connection.Capabilities.Contains("ui"))
            return true;
        setState(UiPreviewClientState.Faulted, "UiPreviewHost does not support UI preview.");
        return false;
    }

    public async Task<UiPreviewFrame?> RenderAsync(
        string assetKey,
        JsonObject asset,
        IReadOnlyDictionary<string, JsonObject> dependencies,
        double renderScale,
        UiPreviewAnimationSample? animation = null,
        CancellationToken cancellationToken = default)
    {
        if (!double.IsFinite(renderScale) || renderScale <= 0)
            throw new ArgumentOutOfRangeException(nameof(renderScale));
        string normalizedAssetKey = UiAssetSchema.NormalizeAssetKey(assetKey);
        if (!string.Equals(normalizedAssetKey, assetKey, StringComparison.Ordinal))
            throw new ArgumentException("UI asset key must be relative to Data/UI/Assets.", nameof(assetKey));
        if (!await StartAsync(cancellationToken))
            return null;
        cancellationToken.ThrowIfCancellationRequested();
        long requestGeneration = Interlocked.Increment(ref generation);
        JsonObject dependencyData = new();
        foreach (KeyValuePair<string, JsonObject> pair in dependencies)
        {
            if (!string.Equals(
                    UiAssetSchema.NormalizeAssetKey(pair.Key),
                    pair.Key,
                    StringComparison.Ordinal))
            {
                throw new ArgumentException(
                    "UI dependency keys must be relative to Data/UI/Assets.",
                    nameof(dependencies));
            }
            dependencyData[pair.Key] = pair.Value.DeepClone();
        }
        JsonObject request = new()
        {
            ["type"] = "render",
            ["generation"] = requestGeneration,
            ["assetKey"] = normalizedAssetKey,
            ["asset"] = asset.DeepClone(),
            ["dependencies"] = dependencyData,
            ["renderScale"] = renderScale,
        };
        if (animation is not null)
        {
            if (string.IsNullOrWhiteSpace(animation.Name)
                || !double.IsFinite(animation.Time)
                || animation.Time < 0.0)
            {
                throw new ArgumentException("UI preview animation sample is invalid.", nameof(animation));
            }
            request["animationName"] = animation.Name;
            request["animationTarget"] = animation.Target;
            request["animationTime"] = animation.Time;
        }
        setRendering(true);
        string? protocolError = null;
        try
        {
            JsonObject response = await connection.ExchangeAsync(request, CancellationToken.None);
            if (cancellationToken.IsCancellationRequested)
                return null;
            if (!string.Equals(getString(response, "type"), "frame", StringComparison.Ordinal))
            {
                throw new InvalidDataException(
                    getString(response, "message", "UiPreviewHost returned an invalid response."));
            }
            long responseGeneration = response["generation"]?.GetValue<long>() ?? 0;
            return responseGeneration == requestGeneration
                ? readFrame(response, responseGeneration)
                : null;
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception))
        {
            protocolError = exception.Message;
            return null;
        }
        finally
        {
            setRendering(false);
            if (protocolError is not null)
                setState(UiPreviewClientState.Faulted, protocolError);
        }
    }

    public async Task<string?> HitTestAsync(
        long frameGeneration,
        double x,
        double y,
        CancellationToken cancellationToken = default)
    {
        if (disposed || !IsReady || !double.IsFinite(x) || !double.IsFinite(y))
            return null;
        cancellationToken.ThrowIfCancellationRequested();
        JsonObject request = new()
        {
            ["type"] = "hitTest",
            ["generation"] = frameGeneration,
            ["x"] = x,
            ["y"] = y,
        };
        try
        {
            JsonObject response = await connection.ExchangeAsync(request, CancellationToken.None);
            if (cancellationToken.IsCancellationRequested)
                return null;
            if (!string.Equals(getString(response, "type"), "hitTest", StringComparison.Ordinal)
                || response["generation"]?.GetValue<long>() != frameGeneration)
            {
                return null;
            }
            string nodeName = getString(response, "nodeName");
            return nodeName.Length == 0 ? null : nodeName;
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception))
        {
            setState(UiPreviewClientState.Faulted, exception.Message);
            return null;
        }
    }

    public async ValueTask DisposeAsync()
    {
        if (disposed)
            return;
        disposed = true;
        connection.StateChanged -= onConnectionStateChanged;
        await connection.DisposeAsync();
        setState(UiPreviewClientState.Unavailable, string.Empty);
    }

    private UiPreviewFrame readFrame(JsonObject response, long responseGeneration)
    {
        int designWidth = response["designWidth"]?.GetValue<int>() ?? 0;
        int designHeight = response["designHeight"]?.GetValue<int>() ?? 0;
        int width = response["width"]?.GetValue<int>() ?? 0;
        int height = response["height"]?.GetValue<int>() ?? 0;
        double renderScale = getDouble(response, "renderScale");
        int minimumStride = checked(width * 4);
        int stride = response["stride"]?.GetValue<int>() ?? minimumStride;
        if (designWidth <= 0
            || designHeight <= 0
            || width <= 0
            || height <= 0
            || !double.IsFinite(renderScale)
            || renderScale <= 0
            || stride < minimumStride)
        {
            throw new InvalidDataException("UiPreviewHost returned invalid frame dimensions.");
        }
        int byteLength = checked(stride * height);
        byte[] pixels = connection.ReadPixels(response, byteLength);
        List<UiPreviewNodeGeometry> nodes = [];
        if (response["nodes"] is JsonArray nodeData)
        {
            foreach (JsonObject node in nodeData.OfType<JsonObject>())
            {
                nodes.Add(new UiPreviewNodeGeometry(
                    getString(node, "nodeName"),
                    getDouble(node, "x"),
                    getDouble(node, "y"),
                    getDouble(node, "width"),
                    getDouble(node, "height"),
                    getDouble(node, "clipX"),
                    getDouble(node, "clipY"),
                    getDouble(node, "clipWidth"),
                    getDouble(node, "clipHeight"),
                    node["drawOrder"]?.GetValue<int>() ?? 0,
                    node["visible"]?.GetValue<bool>() != false));
            }
        }
        return new UiPreviewFrame(
            designWidth,
            designHeight,
            width,
            height,
            stride,
            renderScale,
            pixels,
            nodes,
            responseGeneration);
    }

    private void onConnectionStateChanged(object? sender, EventArgs args)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => onConnectionStateChanged(sender, args));
            return;
        }
        if (!rendering)
            syncConnectionState();
    }

    private void syncConnectionState()
    {
        UiPreviewClientState state = connection.State switch
        {
            PreviewHostConnectionState.Starting => UiPreviewClientState.Starting,
            PreviewHostConnectionState.Ready => UiPreviewClientState.Ready,
            PreviewHostConnectionState.Faulted => UiPreviewClientState.Faulted,
            _ => UiPreviewClientState.Unavailable,
        };
        setState(state, connection.StatusMessage);
    }

    private void setRendering(bool value)
    {
        rendering = value;
        if (value)
            setState(UiPreviewClientState.Rendering, string.Empty);
        else
            syncConnectionState();
    }

    private void setState(UiPreviewClientState state, string message)
    {
        if (disposed && state != UiPreviewClientState.Unavailable)
            return;
        State = state;
        StatusMessage = message;
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private static string getString(JsonObject value, string propertyName, string fallback = "")
    {
        return value[propertyName]?.GetValue<string>() ?? fallback;
    }

    private static double getDouble(JsonObject value, string propertyName)
    {
        return value[propertyName]?.GetValue<double>() ?? 0;
    }
}
