using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;

namespace Ludork.Services;

internal sealed class MapPreviewClient(PreviewRuntimeService runtime) : IDisposable
{
    private readonly PreviewHostConnection connection = new(runtime);
    private readonly string sessionId = Guid.NewGuid().ToString("N");
    private readonly SemaphoreSlim requests = new(1, 1);
    private string? sceneKey;
    private long revision = -1;
    private long resourceRevision = -1;
    private long materialRevision = -1;
    private MapPreviewSceneDelta.Snapshot? acceptedScene;
    internal JsonObject? LastResponse { get; private set; }

    public async Task<Bitmap> RenderAsync(JsonObject request, JsonObject scene, JsonObject materials, CancellationToken cancellationToken)
    {
        string key = request["sceneKey"]!.GetValue<string>();
        long nextRevision = request["revision"]!.GetValue<long>();
        MapPreviewSceneDelta.Snapshot nextScene = sceneKey == key && revision == nextRevision && acceptedScene is not null
            ? acceptedScene : new(scene);
        await requests.WaitAsync(cancellationToken);
        try
        {
            bool wasReady = connection.IsReady;
            long nextResourceRevision = request["resourceRevision"]!.GetValue<long>();
            long nextMaterialRevision = request["materialRevision"]!.GetValue<long>();
            bool fullScene = !wasReady || sceneKey != key || resourceRevision != nextResourceRevision || acceptedScene is null;
            if (fullScene)
                request["scene"] = nextScene.ToJson();
            else if (revision != nextRevision)
                request["sceneChanges"] = MapPreviewSceneDelta.Create(acceptedScene!, nextScene);
            if (!wasReady || resourceRevision != nextResourceRevision || materialRevision != nextMaterialRevision)
                request["materials"] = materials.DeepClone();
            request["type"] = "renderMap";
            request["sessionId"] = sessionId;
            acceptedScene = null;
            materialRevision = -1;
            if (!wasReady && !await connection.StartAsync(cancellationToken))
                throw new InvalidOperationException(connection.StatusMessage);
            if (!connection.Capabilities.Contains("map") || !connection.Capabilities.Contains("mapActorChanges")
                || !connection.Capabilities.Contains("mapEmitters") || !connection.Capabilities.Contains("mapMaterialSnapshots"))
                throw new InvalidOperationException(LocaleService.Get("MAP_PREVIEW_UNAVAILABLE"));
            (Bitmap bitmap, JsonObject response) = await Task.Run(() => renderAsync(request, cancellationToken), cancellationToken);
            LastResponse = response;
            sceneKey = key;
            revision = nextRevision;
            resourceRevision = nextResourceRevision;
            materialRevision = nextMaterialRevision;
            acceptedScene = nextScene;
            return bitmap;
        }
        finally
        {
            requests.Release();
        }
    }

    private async Task<(Bitmap Bitmap, JsonObject Response)> renderAsync(JsonObject request, CancellationToken cancellationToken)
    {
        JsonObject response = await connection.ExchangeAsync(request, cancellationToken).ConfigureAwait(false);
        if (response["type"]?.GetValue<string>() != "mapFrame"
            || response["sessionId"]?.GetValue<string>() != sessionId
            || response["generation"]?.GetValue<long>() != request["generation"]!.GetValue<long>())
        {
            throw new InvalidDataException(response["message"]?.GetValue<string>() ?? "Invalid map preview response.");
        }
        int width = request["width"]!.GetValue<int>();
        int height = request["height"]!.GetValue<int>();
        if (response["width"]?.GetValue<int>() != width || response["height"]?.GetValue<int>() != height
            || response["stride"]?.GetValue<int>() != checked(width * 4)
            || response["pixelFormat"]?.GetValue<string>() != "Bgra8888Premultiplied")
            throw new InvalidDataException("Invalid map preview frame layout.");
        byte[] pixels = connection.ReadPixels(response, checked(width * height * 4));
        long bitmapStarted = Stopwatch.GetTimestamp();
        Bitmap bitmap = createBitmap(pixels, new PixelSize(width, height));
        response["previewBitmapMs"] = Stopwatch.GetElapsedTime(bitmapStarted).TotalMilliseconds;
        return (bitmap, response);
    }

    internal async Task CloseSceneAsync(string leaseId)
    {
        await requests.WaitAsync();
        try
        {
            if (sceneKey is not string key || !key.StartsWith(leaseId + "/", StringComparison.Ordinal) || !connection.IsReady)
                return;
            acceptedScene = null;
            sceneKey = null;
            JsonObject response = await connection.ExchangeAsync(new JsonObject
            {
                ["type"] = "closeMap", ["sessionId"] = sessionId, ["sceneKey"] = key,
            }, CancellationToken.None);
            if (response["type"]?.GetValue<string>() != "mapClosed"
                || response["sessionId"]?.GetValue<string>() != sessionId
                || response["sceneKey"]?.GetValue<string>() != key)
                throw new InvalidDataException("Invalid map preview close response.");
        }
        catch (Exception exception) when (PreviewHostConnection.IsProtocolException(exception) || exception is InvalidOperationException)
        {
            materialRevision = -1;
            Debug.WriteLine(exception);
        }
        finally
        {
            requests.Release();
        }
    }

    private static Bitmap createBitmap(byte[] pixels, PixelSize size)
    {
        GCHandle handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        try
        {
            return new Bitmap(PixelFormat.Bgra8888, AlphaFormat.Premul, handle.AddrOfPinnedObject(), size, new Vector(96, 96), size.Width * 4);
        }
        finally
        {
            handle.Free();
        }
    }

    public void Dispose() => connection.Dispose();
}
