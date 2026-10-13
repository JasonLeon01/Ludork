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
    private string? sceneKey;
    private long revision = -1;
    private long resourceRevision = -1;
    private MapPreviewSceneDelta.Snapshot? acceptedScene;
    internal JsonObject? LastResponse { get; private set; }

    public async Task<Bitmap> RenderAsync(JsonObject request, JsonObject scene, CancellationToken cancellationToken)
    {
        MapPreviewSceneDelta.Snapshot nextScene = new(scene);
        bool wasReady = connection.IsReady;
        string key = request["sceneKey"]!.GetValue<string>();
        long nextRevision = request["revision"]!.GetValue<long>();
        long nextResourceRevision = request["resourceRevision"]!.GetValue<long>();
        bool fullScene = !wasReady || sceneKey != key || resourceRevision != nextResourceRevision || acceptedScene is null;
        if (fullScene)
            request["scene"] = scene.DeepClone();
        else if (revision != nextRevision)
            request["sceneChanges"] = MapPreviewSceneDelta.Create(acceptedScene!, scene);
        request["type"] = "renderMap";
        request["sessionId"] = sessionId;
        acceptedScene = null;
        if (!wasReady && !await connection.StartAsync(cancellationToken).ConfigureAwait(false))
            throw new InvalidOperationException(connection.StatusMessage);
        if (!connection.Capabilities.Contains("map") || !connection.Capabilities.Contains("mapActorChanges"))
            throw new InvalidOperationException(LocaleService.Get("MAP_PREVIEW_UNAVAILABLE"));
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
        LastResponse = response;
        sceneKey = key;
        revision = nextRevision;
        resourceRevision = nextResourceRevision;
        acceptedScene = nextScene;
        return bitmap;
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
