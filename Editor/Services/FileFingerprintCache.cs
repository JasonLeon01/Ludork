using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;

namespace Ludork.Services;

internal sealed class FileFingerprintCache(StringComparer comparer)
{
    private readonly Dictionary<string, Entry> entries = new(comparer);

    internal byte[] Read(string path, byte[] buffer, CancellationToken token, FileShare share = FileShare.Read)
        => read(path, buffer, token, share, false, null).Digest;

    internal (byte[] Digest, byte[] Content) ReadContent(string path, byte[] buffer, CancellationToken token,
        FileShare share = FileShare.Read)
    {
        (byte[] digest, byte[]? content) = read(path, buffer, token, share, true, null);
        return (digest, content!);
    }

    internal (byte[] Digest, byte[]? Content) ReadChangedContent(string path, byte[] buffer, CancellationToken token,
        string? previousFingerprint)
        => read(path, buffer, token, FileShare.Read, true, previousFingerprint);

    internal void RetainPaths(IEnumerable<string> paths)
    {
        HashSet<string> retained = paths.ToHashSet(entries.Comparer);
        lock (entries)
            foreach (string path in entries.Keys.Where(path => !retained.Contains(path)).ToArray())
                entries.Remove(path);
    }

    private (byte[] Digest, byte[]? Content) read(string path, byte[] buffer, CancellationToken token,
        FileShare share, bool includeContent, string? previousFingerprint)
    {
        token.ThrowIfCancellationRequested();
        using FileStream stream = new(path, FileMode.Open, FileAccess.Read, share, 1, FileOptions.SequentialScan);
        FileContentVersion? version = FileContentVersion.Read(stream.SafeFileHandle);
        lock (entries)
        {
            if (!version.HasValue)
                entries.Remove(path);
            else if (entries.TryGetValue(path, out Entry? cached) && cached.Version == version.Value
                && (!includeContent || matches(cached.Digest, previousFingerprint)))
                return (cached.Digest, null);
        }

        long length = includeContent ? stream.Length : 0;
        if (length > Array.MaxLength)
            throw new IOException($"File is too large to load into memory: {path}");
        using MemoryStream? content = includeContent ? new((int)length) : null;
        using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA256);
        int count;
        while ((count = stream.Read(buffer)) != 0)
        {
            token.ThrowIfCancellationRequested();
            hash.AppendData(buffer, 0, count);
            content?.Write(buffer, 0, count);
        }
        token.ThrowIfCancellationRequested();
        byte[] digest = hash.GetHashAndReset();
        if (version.HasValue)
        {
            if (FileContentVersion.Read(stream.SafeFileHandle) != version)
                throw new IOException($"File changed while it was being read: {path}");
            lock (entries)
                entries[path] = new(version.Value, digest);
        }
        if (content is null || matches(digest, previousFingerprint))
            return (digest, null);
        return (digest, content.Length == content.Capacity ? content.GetBuffer() : content.ToArray());
    }

    private static bool matches(byte[] digest, string? fingerprint) =>
        fingerprint is not null && Convert.ToHexString(digest).Equals(fingerprint, StringComparison.Ordinal);

    private sealed record Entry(FileContentVersion Version, byte[] Digest);
}
