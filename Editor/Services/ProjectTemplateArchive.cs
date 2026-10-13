using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Formats.Tar;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal static class ProjectTemplateArchive
{
    private static readonly uint[] crc32Table = createCrc32Table();

    public static async Task ExtractAsync(string archivePath, string targetPath, bool zip,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        progress.Report(new(ProjectTemplateStage.Extracting, 0, null));
        List<ArchiveLink> links = [];
        List<(string Path, UnixFileMode Mode)> directoryModes = [];
        await using FileStream source = new(archivePath, FileMode.Open, FileAccess.Read,
            FileShare.Read, 65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        try
        {
            if (zip)
                await extractZipAsync(source, targetPath, links, directoryModes, progress, cancellationToken);
            else
                await extractTarAsync(source, targetPath, links, directoryModes, progress, cancellationToken);
        }
        catch (EndOfStreamException exception)
        {
            throw new InvalidDataException(LocaleService.Get("NEW_PROJECT_TEMPLATE_INVALID"), exception);
        }
        installLinks(targetPath, links, cancellationToken);
        applyDirectoryModes(directoryModes);
    }

    public static async Task CopyDirectoryAsync(string sourcePath, string targetPath,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        progress.Report(new(ProjectTemplateStage.Copying, 0, null));
        string sourceRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(sourcePath));
        if (targetPath.StartsWith(sourceRoot + Path.DirectorySeparatorChar,
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
            throw invalidArchive(targetPath);
        List<FileSystemInfo> entries = [];
        Stack<DirectoryInfo> directories = new();
        directories.Push(new DirectoryInfo(sourcePath));
        long total = 0;
        while (directories.Count != 0)
        {
            foreach (FileSystemInfo entry in directories.Pop().EnumerateFileSystemInfos())
            {
                cancellationToken.ThrowIfCancellationRequested();
                entries.Add(entry);
                if (entry.LinkTarget is not null)
                    continue;
                if (entry is DirectoryInfo directory)
                    directories.Push(directory);
                else if (entry is FileInfo file)
                    total += file.Length;
            }
        }
        List<ArchiveLink> links = [];
        List<(string Path, UnixFileMode Mode)> directoryModes = [];
        long completed = 0;
        foreach (FileSystemInfo entry in entries)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string relative = Path.GetRelativePath(sourcePath, entry.FullName).Replace('\\', '/');
            string path = entryPath(targetPath, relative);
            if (entry.LinkTarget is string linkTarget)
            {
                links.Add(new(path, linkTarget, false));
                continue;
            }
            if (entry is DirectoryInfo)
            {
                Directory.CreateDirectory(path);
                if (!OperatingSystem.IsWindows())
                    directoryModes.Add((path, File.GetUnixFileMode(entry.FullName)));
                continue;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            await using FileStream input = new(entry.FullName, FileMode.Open, FileAccess.Read,
                FileShare.Read, 65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
            await copyFileAsync(input, path, chunk =>
            {
                completed += chunk.Length;
                progress.Report(new(ProjectTemplateStage.Copying, completed, total));
            }, cancellationToken);
            if (!OperatingSystem.IsWindows())
                File.SetUnixFileMode(path, File.GetUnixFileMode(entry.FullName));
        }
        installLinks(targetPath, links, cancellationToken);
        applyDirectoryModes(directoryModes);
        progress.Report(new(ProjectTemplateStage.Copying, completed, total));
    }

    private static async Task extractZipAsync(Stream source, string targetPath,
        List<ArchiveLink> links, List<(string Path, UnixFileMode Mode)> directoryModes,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        using ZipArchive archive = new(source, ZipArchiveMode.Read, true);
        long total = archive.Entries.Sum(entry => entry.Length);
        long completed = 0;
        foreach (ZipArchiveEntry entry in archive.Entries)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string path = entryPath(targetPath, entry.FullName);
            int type = (entry.ExternalAttributes >> 16) & 0xF000;
            UnixFileMode mode = (UnixFileMode)((entry.ExternalAttributes >> 16) & 0x1FF);
            if (entry.FullName.EndsWith('/') || type == 0x4000)
            {
                Directory.CreateDirectory(path);
                if (mode != 0)
                    directoryModes.Add((path, mode));
                continue;
            }
            requireFilePath(targetPath, path);
            if (type == 0xA000)
            {
                await using ChecksumReadStream linkStream = new(entry.Open());
                using MemoryStream linkBytes = new();
                await linkStream.CopyToAsync(linkBytes, cancellationToken);
                if (linkBytes.Length != entry.Length
                    || linkStream.Checksum != entry.Crc32)
                    throw invalidArchive(entry.FullName);
                string target = Encoding.UTF8.GetString(linkBytes.GetBuffer(), 0, (int)linkBytes.Length);
                links.Add(new(path, target, false));
                completed += entry.Length;
                continue;
            }
            if (type != 0 && type != 0x8000)
                throw invalidArchive(entry.FullName);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            await using ChecksumReadStream input = new(entry.Open());
            long length = await copyFileAsync(input, path, chunk =>
            {
                completed += chunk.Length;
                progress.Report(new(ProjectTemplateStage.Extracting, completed, total));
            }, cancellationToken);
            if (length != entry.Length || input.Checksum != entry.Crc32)
                throw invalidArchive(entry.FullName);
            if (!OperatingSystem.IsWindows() && mode != 0)
                File.SetUnixFileMode(path, mode);
        }
        progress.Report(new(ProjectTemplateStage.Extracting, completed, total));
    }

    private static async Task extractTarAsync(FileStream source, string targetPath,
        List<ArchiveLink> links, List<(string Path, UnixFileMode Mode)> directoryModes,
        IProgress<ProjectTemplateProgress> progress, CancellationToken cancellationToken)
    {
        if (source.Length < 18)
            throw invalidArchive(source.Name);
        byte[] footer = new byte[8];
        source.Seek(-footer.Length, SeekOrigin.End);
        await source.ReadExactlyAsync(footer, cancellationToken);
        source.Position = 0;
        await using ChecksumReadStream gzip = new(new GZipStream(source, CompressionMode.Decompress, true));
        await using TarReader reader = new(gzip, true);
        TarEntry? entry;
        while ((entry = await reader.GetNextEntryAsync(false, cancellationToken)) is not null)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string path = entryPath(targetPath, entry.Name);
            if (entry.EntryType == TarEntryType.Directory)
            {
                Directory.CreateDirectory(path);
                directoryModes.Add((path, entry.Mode));
                continue;
            }
            requireFilePath(targetPath, path);
            if (entry.EntryType is TarEntryType.SymbolicLink or TarEntryType.HardLink)
            {
                links.Add(new(path, entry.LinkName, entry.EntryType == TarEntryType.HardLink));
                continue;
            }
            if (entry.EntryType is not TarEntryType.RegularFile and not TarEntryType.V7RegularFile)
                throw invalidArchive(entry.Name);
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            long length = await copyFileAsync(entry.DataStream ?? Stream.Null, path,
                _ => progress.Report(new(ProjectTemplateStage.Extracting, source.Position, source.Length)),
                cancellationToken);
            if (length != entry.Length)
                throw invalidArchive(entry.Name);
            if (!OperatingSystem.IsWindows())
                File.SetUnixFileMode(path, entry.Mode);
        }
        byte[] buffer = new byte[65536];
        while (await gzip.ReadAsync(buffer, cancellationToken) != 0)
        {
            cancellationToken.ThrowIfCancellationRequested();
        }
        if (gzip.Checksum != BinaryPrimitives.ReadUInt32LittleEndian(footer)
            || unchecked((uint)gzip.BytesRead) != BinaryPrimitives.ReadUInt32LittleEndian(footer.AsSpan(4)))
            throw invalidArchive(source.Name);
        progress.Report(new(ProjectTemplateStage.Extracting, source.Length, source.Length));
    }

    private static async Task<long> copyFileAsync(Stream source, string path,
        Action<ReadOnlyMemory<byte>> report, CancellationToken cancellationToken)
    {
        await using FileStream target = new(path, FileMode.CreateNew, FileAccess.Write,
            FileShare.None, 65536, FileOptions.Asynchronous | FileOptions.SequentialScan);
        byte[] buffer = new byte[65536];
        long completed = 0;
        int count;
        while ((count = await source.ReadAsync(buffer, cancellationToken)) != 0)
        {
            await target.WriteAsync(buffer.AsMemory(0, count), cancellationToken);
            completed += count;
            report(buffer.AsMemory(0, count));
        }
        return completed;
    }

    private static string entryPath(string root, string name)
    {
        if (string.IsNullOrEmpty(name) || name.Contains('\\') || Path.IsPathRooted(name))
            throw invalidArchive(name);
        string[] parts = name.Split('/');
        if (parts.Any(part => part == ".." || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0))
            throw invalidArchive(name);
        string path = Path.GetFullPath(Path.Combine(root, name));
        requireInsideRoot(root, path);
        return path;
    }

    private static void requireInsideRoot(string root, string path)
    {
        string normalizedRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
        StringComparison comparison = OperatingSystem.IsWindows()
            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        if (!string.Equals(path, normalizedRoot, comparison)
            && !path.StartsWith(normalizedRoot + Path.DirectorySeparatorChar, comparison))
            throw invalidArchive(path);
    }

    private static void requireFilePath(string root, string path)
    {
        if (string.Equals(Path.TrimEndingDirectorySeparator(root), path,
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal))
            throw invalidArchive(path);
    }

    private static void installLinks(string root, List<ArchiveLink> links,
        CancellationToken cancellationToken)
    {
        Dictionary<string, ArchiveLink> byPath = new(OperatingSystem.IsWindows()
            ? StringComparer.OrdinalIgnoreCase : StringComparer.Ordinal);
        foreach (ArchiveLink link in links)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (string.IsNullOrWhiteSpace(link.Target) || Path.IsPathRooted(link.Target)
                || link.Target.Contains('\\') || Path.Exists(link.Path) || !byPath.TryAdd(link.Path, link))
                throw invalidArchive(link.Path);
        }
        string resolveLink(ArchiveLink link, HashSet<string> visited)
        {
            if (!visited.Add(link.Path))
                throw invalidArchive(link.Path);
            string basePath = link.HardLink ? root : Path.GetDirectoryName(link.Path)!;
            string target = Path.GetFullPath(Path.Combine(basePath, link.Target));
            requireInsideRoot(root, target);
            if (byPath.TryGetValue(target, out ArchiveLink? next))
                return resolveLink(next, visited);
            if (!File.Exists(target) || (File.GetAttributes(target) & FileAttributes.ReparsePoint) != 0)
                throw invalidArchive(link.Path);
            return target;
        }
        foreach (ArchiveLink link in links)
        {
            cancellationToken.ThrowIfCancellationRequested();
            string source = resolveLink(link, new HashSet<string>(byPath.Comparer));
            Directory.CreateDirectory(Path.GetDirectoryName(link.Path)!);
            if (link.HardLink)
                File.Copy(source, link.Path);
            else
                File.CreateSymbolicLink(link.Path, link.Target);
        }
    }

    private static void applyDirectoryModes(List<(string Path, UnixFileMode Mode)> directories)
    {
        if (OperatingSystem.IsWindows())
            return;
        foreach ((string path, UnixFileMode mode) in directories.OrderByDescending(entry => entry.Path.Length))
            File.SetUnixFileMode(path, mode);
    }

    private static InvalidDataException invalidArchive(string entry)
    {
        return new InvalidDataException(LocaleService.Get("NEW_PROJECT_TEMPLATE_INVALID") + " " + entry);
    }

    private static uint[] createCrc32Table()
    {
        uint[] table = new uint[256];
        for (uint index = 0; index < table.Length; index++)
        {
            uint value = index;
            for (int bit = 0; bit < 8; bit++)
                value = (value >> 1) ^ ((value & 1) != 0 ? 0xEDB88320u : 0);
            table[index] = value;
        }
        return table;
    }

    private static uint appendCrc32(uint checksum, ReadOnlySpan<byte> bytes)
    {
        foreach (byte value in bytes)
            checksum = (checksum >> 8) ^ crc32Table[(checksum ^ value) & 0xFF];
        return checksum;
    }

    private sealed record ArchiveLink(string Path, string Target, bool HardLink);

    private sealed class ChecksumReadStream(Stream source) : Stream
    {
        private uint checksum = uint.MaxValue;

        public uint Checksum => ~checksum;
        public long BytesRead { get; private set; }
        public override bool CanRead => true;
        public override bool CanSeek => false;
        public override bool CanWrite => false;
        public override long Length => throw new NotSupportedException();
        public override long Position
        {
            get => throw new NotSupportedException();
            set => throw new NotSupportedException();
        }

        public override int Read(byte[] buffer, int offset, int count)
        {
            return Read(buffer.AsSpan(offset, count));
        }

        public override int Read(Span<byte> buffer)
        {
            int count = source.Read(buffer);
            record(buffer[..count]);
            return count;
        }

        public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
        {
            int count = await source.ReadAsync(buffer, cancellationToken);
            record(buffer.Span[..count]);
            return count;
        }

        public override Task<int> ReadAsync(byte[] buffer, int offset, int count, CancellationToken cancellationToken)
        {
            return ReadAsync(buffer.AsMemory(offset, count), cancellationToken).AsTask();
        }

        private void record(ReadOnlySpan<byte> bytes)
        {
            checksum = appendCrc32(checksum, bytes);
            BytesRead += bytes.Length;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
                source.Dispose();
            base.Dispose(disposing);
        }

        public override ValueTask DisposeAsync() => source.DisposeAsync();
        public override void Flush() => throw new NotSupportedException();
        public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
        public override void SetLength(long value) => throw new NotSupportedException();
        public override void Write(byte[] buffer, int offset, int count) => throw new NotSupportedException();
    }
}
