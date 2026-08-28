using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public sealed partial class ActorPreviewService
{
    private async Task<ActorPreviewBatchFrame?> requestFrameAsync(
        IReadOnlyList<ActorPreviewLease> active,
        TimeSpan elapsed,
        CancellationToken cancellationToken)
    {
        long requestGeneration = Interlocked.Increment(ref generation);
        JsonArray items = [];
        Dictionary<string, ActorVisualDescriptor> descriptors = new(StringComparer.Ordinal);
        foreach (ActorPreviewLease lease in active)
        {
            ActorVisualDescriptor descriptor = lease.Descriptor;
            PixelRect textureRect = lease.getTextureRect(elapsed);
            string id = createRequestId(descriptor, textureRect);
            if (!descriptors.TryAdd(id, descriptor))
                continue;
            items.Add(new JsonObject
            {
                ["id"] = id,
                ["texturePath"] = descriptor.TexturePath,
                ["textureRect"] = new JsonObject
                {
                    ["x"] = textureRect.X,
                    ["y"] = textureRect.Y,
                    ["width"] = textureRect.Width,
                    ["height"] = textureRect.Height,
                },
                ["shaderPath"] = descriptor.ShaderPath,
                ["hue"] = normalizeHue(descriptor.Hue),
            });
        }
        JsonObject request = new()
        {
            ["type"] = "renderActorBatch",
            ["generation"] = requestGeneration,
            ["time"] = elapsed.TotalSeconds,
            ["items"] = items,
        };
        JsonObject response = await connection.ExchangeAsync(request, cancellationToken);
        cancellationToken.ThrowIfCancellationRequested();
        if (!string.Equals(getString(response, "type"), "actorFrame", StringComparison.Ordinal))
            throw new InvalidDataException(getString(response, "message", "UiPreviewHost returned an invalid actor frame."));
        long responseGeneration = response["generation"]?.GetValue<long>() ?? 0;
        if (responseGeneration != requestGeneration)
            return null;
        if (!string.Equals(getString(response, "pixelFormat"), "Bgra8888Premultiplied", StringComparison.Ordinal))
            throw new InvalidDataException("UiPreviewHost returned an unsupported actor pixel format.");
        List<ActorPreviewAtlasPage> pages = [];
        if (response["pages"] is JsonArray pageData)
        {
            foreach (JsonObject page in pageData.OfType<JsonObject>())
            {
                cancellationToken.ThrowIfCancellationRequested();
                int width = page["width"]?.GetValue<int>() ?? 0;
                int height = page["height"]?.GetValue<int>() ?? 0;
                int stride = page["stride"]?.GetValue<int>() ?? width * 4;
                if (width <= 0 || height <= 0 || stride < width * 4)
                    throw new InvalidDataException("UiPreviewHost returned invalid actor atlas dimensions.");
                pages.Add(new ActorPreviewAtlasPage(
                    width,
                    height,
                    stride,
                    connection.ReadPixels(page, checked(stride * height))));
            }
        }
        List<ActorPreviewAtlasItem> resultItems = [];
        if (response["items"] is JsonArray itemData)
        {
            foreach (JsonObject item in itemData.OfType<JsonObject>())
            {
                resultItems.Add(new ActorPreviewAtlasItem(
                    getString(item, "id"),
                    item["page"]?.GetValue<int>() ?? -1,
                    new PixelRect(
                        item["x"]?.GetValue<int>() ?? 0,
                        item["y"]?.GetValue<int>() ?? 0,
                        item["width"]?.GetValue<int>() ?? 0,
                        item["height"]?.GetValue<int>() ?? 0),
                    item["shaderError"]?.GetValue<bool>() == true,
                    getString(item, "error")));
            }
        }
        return new ActorPreviewBatchFrame(responseGeneration, pages, resultItems);
    }

    private void publishFrame(
        ActorPreviewBatchFrame frame,
        IReadOnlyList<ActorPreviewLease> active,
        TimeSpan elapsed)
    {
        if (disposed || frame.Generation != Interlocked.Read(ref generation))
            return;
        Dictionary<string, ActorPreviewAtlasItem> items = frame.Items.ToDictionary(item => item.Id, StringComparer.Ordinal);
        List<(ActorPreviewLease Lease, ActorPreviewAtlasItem Item, PixelRect TextureRect, ActorPreviewAtlasPage Page)>
            publications = [];
        foreach (ActorPreviewLease lease in active)
        {
            if (lease.IsDisposed || !lease.IsActive)
                continue;
            ActorVisualDescriptor descriptor = lease.Descriptor;
            PixelRect textureRect = lease.getTextureRect(elapsed);
            string id = createRequestId(descriptor, textureRect);
            if (!items.TryGetValue(id, out ActorPreviewAtlasItem? item)
                || item.Page < 0 || item.Page >= frame.Pages.Count
                || item.Rect.Width <= 0 || item.Rect.Height <= 0)
            {
                if (lease.IsStatic && descriptor.ShaderPath.Length != 0)
                {
                    string message = "UiPreviewHost did not return the actor shader preview.";
                    if (lease.Frame is not null)
                        lease.publishRenderError(textureRect, message);
                    else if (!updateFallback(lease, textureRect, true, message))
                        lease.clearFrame(textureRect, message);
                }
                else
                    updateFallback(lease, textureRect, true);
                continue;
            }
            ActorPreviewAtlasPage page = frame.Pages[item.Page];
            if (item.Rect.X < 0 || item.Rect.Y < 0
                || item.Rect.Right > page.Width || item.Rect.Bottom > page.Height)
                throw new InvalidDataException("UiPreviewHost returned an actor atlas item outside its page.");
            publications.Add((lease, item, textureRect, page));
        }
        bool needsAtlas = active.Any(lease => lease.UsesAtlas);
        List<WriteableBitmap> nextAtlasPages = needsAtlas
            ? frame.Pages.Select(createAtlasBitmap).ToList()
            : [];
        List<WriteableBitmap> previousAtlasPages = [];
        if (needsAtlas)
        {
            previousAtlasPages = atlasPages;
            atlasPages = nextAtlasPages;
        }
        foreach ((ActorPreviewLease lease, ActorPreviewAtlasItem item, PixelRect textureRect, ActorPreviewAtlasPage page)
            in publications)
        {
            string? shaderError = item.ShaderError ? item.Error : null;
            if (lease.UsesAtlas)
            {
                lease.publishAtlas(
                    nextAtlasPages[item.Page],
                    item.Rect,
                    textureRect,
                    shaderError);
            }
            else
            {
                lease.publish(
                    textureRect,
                    item.Rect.Width,
                    item.Rect.Height,
                    page.Stride,
                    page.Pixels,
                    item.Rect.X,
                    item.Rect.Y,
                    true,
                    shaderError);
            }
        }
        retireAtlasPages(previousAtlasPages);
    }

    private static WriteableBitmap createAtlasBitmap(ActorPreviewAtlasPage page)
    {
        WriteableBitmap bitmap = new(
            new PixelSize(page.Width, page.Height),
            new Vector(96, 96),
            PixelFormat.Bgra8888,
            AlphaFormat.Premul);
        using (ILockedFramebuffer frame = bitmap.Lock())
        {
            byte[] target = new byte[frame.RowBytes * page.Height];
            for (int y = 0; y < page.Height; y += 1)
            {
                Buffer.BlockCopy(
                    page.Pixels,
                    y * page.Stride,
                    target,
                    y * frame.RowBytes,
                    page.Width * 4);
            }
            Marshal.Copy(target, 0, frame.Address, target.Length);
        }
        return bitmap;
    }

    private void retireAtlasPages(IReadOnlyList<WriteableBitmap> pages)
    {
        if (pages.Count == 0)
            return;
        foreach (WriteableBitmap page in pages)
            retiredAtlasPages.Add(page);
        Dispatcher.UIThread.Post(
            () =>
            {
                foreach (WriteableBitmap page in pages)
                {
                    if (retiredAtlasPages.Remove(page))
                        page.Dispose();
                }
            },
            DispatcherPriority.Background);
    }
}
