using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugin.Abstractions;

public static class FilePersistence
{
    private const int retryMilliseconds = 10_000;
    private const int initialDelayMilliseconds = 50;
    private const int maximumDelayMilliseconds = 400;

    private static void run(Action action)
    {
        if (!OperatingSystem.IsWindows())
        {
            action();
            return;
        }

        long deadline = Environment.TickCount64 + retryMilliseconds;
        int delay = initialDelayMilliseconds;
        while (true)
        {
            try
            {
                action();
                return;
            }
            catch (Exception exception) when (
                isTransient(exception) && Environment.TickCount64 < deadline)
            {
                Thread.Sleep(delay);
                delay = Math.Min(delay * 2, maximumDelayMilliseconds);
            }
        }
    }

    public static void MoveFile(string source, string destination, bool overwrite = false)
    {
        run(() =>
        {
            if (overwrite)
                clearReadOnly(destination);
            File.Move(source, destination, overwrite);
        });
    }

    public static void DeleteFile(string path)
    {
        run(() =>
        {
            clearReadOnly(path);
            File.Delete(path);
        });
    }

    public static void MoveDirectory(string source, string destination)
    {
        run(() => Directory.Move(source, destination));
    }

    public static string CreateTemporaryPath(string destination)
    {
        string directory = Path.GetDirectoryName(Path.GetFullPath(destination))!;
        return Path.Combine(directory, "." + Path.GetFileName(destination) + "." + Guid.NewGuid().ToString("N") + ".tmp");
    }

    public static void WriteDurable(string path, Action<Stream> write)
    {
        using FileStream stream = new(path, FileMode.CreateNew, FileAccess.Write, FileShare.None);
        write(stream);
        stream.Flush(true);
    }

    public static void WriteAtomic(string destination, Action<Stream> write, Action? beforeCommit = null)
    {
        string temporary = CreateTemporaryPath(destination);
        Directory.CreateDirectory(Path.GetDirectoryName(temporary)!);
        Exception? failure = null;
        try
        {
            WriteDurable(temporary, write);
            beforeCommit?.Invoke();
            MoveFile(temporary, destination, true);
        }
        catch (Exception exception)
        {
            failure = exception;
            throw;
        }
        finally
        {
            deleteTemporary(temporary, failure);
        }
    }

    public static void WriteAllTextAtomic(string destination, string content)
    {
        WriteAtomic(destination, stream =>
        {
            using StreamWriter writer = new(stream, new UTF8Encoding(false), leaveOpen: true);
            writer.Write(content);
        });
    }

    public static async Task WriteAtomicAsync(
        string destination,
        Func<Stream, CancellationToken, Task> write,
        CancellationToken cancellationToken)
    {
        cancellationToken.ThrowIfCancellationRequested();
        string temporary = CreateTemporaryPath(destination);
        Directory.CreateDirectory(Path.GetDirectoryName(temporary)!);
        Exception? failure = null;
        try
        {
            await using (FileStream stream = new(temporary, FileMode.CreateNew, FileAccess.Write,
                FileShare.None, 4096, FileOptions.Asynchronous | FileOptions.WriteThrough))
            {
                await write(stream, cancellationToken);
                await stream.FlushAsync(cancellationToken);
                stream.Flush(true);
            }
            cancellationToken.ThrowIfCancellationRequested();
            MoveFile(temporary, destination, true);
        }
        catch (Exception exception)
        {
            failure = exception;
            throw;
        }
        finally
        {
            deleteTemporary(temporary, failure);
        }
    }

    private static void deleteTemporary(string temporary, Exception? failure)
    {
        try
        {
            if (File.Exists(temporary))
                DeleteFile(temporary);
        }
        catch (Exception exception) when (failure is not null
            && exception is IOException or UnauthorizedAccessException)
        {
            throw new IOException(failure.Message + "; temporary file cleanup failed: " + exception.Message, failure);
        }
    }

    public static Task WriteAllTextAtomicAsync(
        string destination,
        string content,
        CancellationToken cancellationToken)
    {
        return WriteAtomicAsync(destination, async (stream, token) =>
        {
            await using StreamWriter writer = new(stream, new UTF8Encoding(false), leaveOpen: true);
            await writer.WriteAsync(content.AsMemory(), token);
            await writer.FlushAsync(token);
        }, cancellationToken);
    }

    private static void clearReadOnly(string path)
    {
        if (!File.Exists(path))
            return;
        FileAttributes attributes = File.GetAttributes(path);
        if ((attributes & FileAttributes.ReadOnly) != 0)
            File.SetAttributes(path, attributes & ~FileAttributes.ReadOnly);
    }

    private static bool isTransient(Exception exception)
    {
        int code = exception.HResult & 0xFFFF;
        return exception is UnauthorizedAccessException or IOException
            && code is 5 or 32 or 33;
    }
}
