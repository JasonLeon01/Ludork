using Avalonia.Media;
using Avalonia.Media.Imaging;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class MediaFileThumbnail
{
    public static bool CanLoad(string path) => MediaFileTypes.GetContentType(path) is not null;

    public static IImage GetPlaceholder(string path) => EditorIconResources.GetImage(
        MediaFileTypes.IsAudio(path) ? "EditorImage.Audio" : MediaFileTypes.IsVideo(path) ? "EditorImage.Video" : "EditorImage.File");

    public static Task<EditorThumbnailLease?> AcquireAsync(EditorThumbnailService thumbnails, string path,
        int pixelSize, CancellationToken token)
    {
        bool video = MediaFileTypes.IsVideo(path);
        int size = Math.Clamp(pixelSize, 16, 1024);
        return thumbnails.AcquireAsync(path, size, video ? "video-cover" : "audio-icon", () =>
        {
            if (OperatingSystem.IsMacOS())
                return video ? MacOSVideoThumbnail.Create(path, size) : MacOSFileIconBackend.GetAudioIcon(size);
            if (OperatingSystem.IsWindows())
                return video ? WindowsVideoThumbnail.Create(path, size)
                    : new WindowsFileIconBackend().GetIcon("ludork-audio.wav", false, size) as Bitmap;
            return null;
        }, token);
    }
}
