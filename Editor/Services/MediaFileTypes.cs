using System;
using System.IO;

namespace Ludork.Services;

internal static class MediaFileTypes
{
    public static string? GetContentType(string path) => Path.GetExtension(path).ToLowerInvariant() switch
    {
        ".ogg" or ".oga" => "audio/ogg",
        ".wav" => "audio/wav",
        ".flac" => "audio/flac",
        ".mp3" => "audio/mpeg",
        ".m4a" => "audio/mp4",
        ".aac" => "audio/aac",
        ".opus" => "audio/ogg",
        ".mp4" or ".m4v" => "video/mp4",
        ".mov" => "video/quicktime",
        ".webm" => "video/webm",
        ".ogv" => "video/ogg",
        ".mkv" => "video/x-matroska",
        ".avi" => "video/x-msvideo",
        ".wmv" => "video/x-ms-wmv",
        _ => null,
    };

    public static bool IsAudio(string path) => GetContentType(path)?.StartsWith("audio/", StringComparison.Ordinal) == true;

    public static bool IsVideo(string path) => GetContentType(path)?.StartsWith("video/", StringComparison.Ordinal) == true;
}
