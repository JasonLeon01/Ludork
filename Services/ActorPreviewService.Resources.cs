using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace Ludork.Services;

public sealed partial class ActorPreviewService
{
    private SourceTexture? getSourceTexture(string assetPath)
    {
        if (disposed
            || !GameAssetPath.TryResolveExistingFile(
                projectPath,
                assetPath,
                out string filePath))
        {
            return null;
        }
        FileInfo info = new(filePath);
        sourceTextureAccessOrder += 1;
        if (sourceTextures.TryGetValue(assetPath, out CachedSourceTexture? cached)
            && cached.Texture.LastWriteTimeUtc == info.LastWriteTimeUtc
            && cached.Texture.Length == info.Length)
        {
            cached.LastUsed = sourceTextureAccessOrder;
            return cached.Texture;
        }
        if (cached is not null)
        {
            sourceTextures.Remove(assetPath);
            sourceTextureBytes -= cached.Texture.Bytes;
            cached.Texture.Dispose();
        }
        SourceTexture loaded = SourceTexture.Load(
            filePath,
            info.LastWriteTimeUtc,
            info.Length);
        sourceTextures[assetPath] = new CachedSourceTexture(
            loaded,
            sourceTextureAccessOrder);
        sourceTextureBytes += loaded.Bytes;
        trimSourceTextures(assetPath);
        return loaded;
    }

    private void trimSourceTextures(string retainedPath)
    {
        if (sourceTextures.Count <= MaximumSourceTextures
            && sourceTextureBytes <= MaximumSourceTextureBytes)
        {
            return;
        }
        foreach (string path in sourceTextures
                     .Where(entry => !string.Equals(entry.Key, retainedPath, StringComparison.Ordinal))
                     .OrderBy(entry => entry.Value.LastUsed)
                     .Select(entry => entry.Key)
                     .ToArray())
        {
            if (sourceTextures.Count <= MaximumSourceTextures
                && sourceTextureBytes <= MaximumSourceTextureBytes)
            {
                break;
            }
            CachedSourceTexture removed = sourceTextures[path];
            sourceTextures.Remove(path);
            sourceTextureBytes -= removed.Texture.Bytes;
            removed.Texture.Dispose();
        }
    }

    private sealed class CachedSourceTexture(
        SourceTexture texture,
        long lastUsed)
    {
        public SourceTexture Texture { get; } = texture;
        public long LastUsed { get; set; } = lastUsed;
    }

    private sealed class SourceTexture : IDisposable
    {
        private readonly byte[] pixels;

        private SourceTexture(
            int width,
            int height,
            int stride,
            byte[] pixels,
            DateTime lastWriteTimeUtc,
            long length)
        {
            Width = width;
            Height = height;
            Stride = stride;
            this.pixels = pixels;
            LastWriteTimeUtc = lastWriteTimeUtc;
            Length = length;
        }

        public int Width { get; }
        public int Height { get; }
        public int Stride { get; }
        public long Bytes => pixels.LongLength;
        public DateTime LastWriteTimeUtc { get; }
        public long Length { get; }

        public static SourceTexture Load(string filePath, DateTime lastWriteTimeUtc, long length)
        {
            using Bitmap bitmap = new(filePath);
            using WriteableBitmap buffer = new(
                bitmap.PixelSize,
                bitmap.Dpi,
                PixelFormat.Bgra8888,
                AlphaFormat.Unpremul);
            using ILockedFramebuffer frame = buffer.Lock();
            bitmap.CopyPixels(frame);
            byte[] pixels = new byte[frame.RowBytes * frame.Size.Height];
            Marshal.Copy(frame.Address, pixels, 0, pixels.Length);
            int width = frame.Size.Width;
            int height = frame.Size.Height;
            int stride = frame.RowBytes;
            return new SourceTexture(width, height, stride, pixels, lastWriteTimeUtc, length);
        }

        public byte[] Copy(PixelRect rect, double hue)
        {
            byte[] result = new byte[rect.Width * rect.Height * 4];
            for (int y = 0; y < rect.Height; y += 1)
            {
                int sourceOffset = (rect.Y + y) * Stride + rect.X * 4;
                int targetOffset = y * rect.Width * 4;
                Buffer.BlockCopy(pixels, sourceOffset, result, targetOffset, rect.Width * 4);
            }
            if (hue > 0.001)
                applyHue(result, hue);
            return result;
        }

        public void Dispose()
        {
        }

        private static void applyHue(byte[] data, double hue)
        {
            float offset = (float)(hue / 360.0);
            for (int index = 0; index < data.Length; index += 4)
            {
                if (data[index + 3] == 0)
                    continue;
                rgbToHsv(data[index + 2], data[index + 1], data[index], out float h, out float s, out float v);
                hsvToRgb((h + offset) % 1f, s, v, out byte r, out byte g, out byte b);
                data[index + 2] = r;
                data[index + 1] = g;
                data[index] = b;
            }
        }

        private static void rgbToHsv(byte r, byte g, byte b, out float h, out float s, out float v)
        {
            float rf = r / 255f;
            float gf = g / 255f;
            float bf = b / 255f;
            float max = Math.Max(rf, Math.Max(gf, bf));
            float min = Math.Min(rf, Math.Min(gf, bf));
            float delta = max - min;
            v = max;
            if (delta <= 0.00001f)
            {
                h = 0;
                s = 0;
                return;
            }
            s = delta / max;
            if (rf >= max)
                h = (gf - bf) / delta % 6f;
            else if (gf >= max)
                h = (bf - rf) / delta + 2f;
            else
                h = (rf - gf) / delta + 4f;
            h /= 6f;
            if (h < 0)
                h += 1f;
        }

        private static void hsvToRgb(float h, float s, float v, out byte r, out byte g, out byte b)
        {
            float c = v * s;
            float x = c * (1 - Math.Abs(h * 6f % 2f - 1));
            float m = v - c;
            float rf;
            float gf;
            float bf;
            int sector = (int)(h * 6f) % 6;
            switch (sector)
            {
                case 0: rf = c; gf = x; bf = 0; break;
                case 1: rf = x; gf = c; bf = 0; break;
                case 2: rf = 0; gf = c; bf = x; break;
                case 3: rf = 0; gf = x; bf = c; break;
                case 4: rf = x; gf = 0; bf = c; break;
                default: rf = c; gf = 0; bf = x; break;
            }
            r = (byte)Math.Clamp((rf + m) * 255f, 0, 255);
            g = (byte)Math.Clamp((gf + m) * 255f, 0, 255);
            b = (byte)Math.Clamp((bf + m) * 255f, 0, 255);
        }
    }
}
