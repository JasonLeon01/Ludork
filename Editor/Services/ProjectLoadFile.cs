using System;
using System.IO;
using System.Text;
using System.Text.Json.Nodes;
using System.Threading;

namespace Ludork.Services;

internal sealed class ProjectLoadFile(string path, FileFingerprintCache cache, byte[] buffer,
    CancellationToken token, bool capture)
{
    internal string Path => path;
    internal CancellationToken CancellationToken => token;
    internal string? Fingerprint { get; private set; }

    internal byte[] ReadContent()
    {
        token.ThrowIfCancellationRequested();
        if (!capture)
            return File.ReadAllBytes(path);
        (byte[] digest, byte[] content) = cache.ReadContent(path, buffer, token);
        Fingerprint = Convert.ToHexString(digest);
        return content;
    }

    internal byte[]? ReadChangedContent(string? previousFingerprint)
    {
        (byte[] digest, byte[]? content) = cache.ReadChangedContent(path, buffer, token, previousFingerprint);
        Fingerprint = Convert.ToHexString(digest);
        return content;
    }

    internal static JsonObject? ParseObject(byte[] content)
    {
        using MemoryStream stream = new(content, false);
        using StreamReader reader = new(stream, Encoding.UTF8, true);
        return JsonNode.Parse(reader.ReadToEnd()) as JsonObject;
    }
}
