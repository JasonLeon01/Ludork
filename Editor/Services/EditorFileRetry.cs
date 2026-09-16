using System;
using System.IO;
using System.Threading;

namespace Ludork.Services;

internal static class EditorFileRetry
{
    private const int retryMilliseconds = 10_000;
    private const int initialDelayMilliseconds = 50;
    private const int maximumDelayMilliseconds = 400;

    public static void Run(Action action)
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
        Run(() =>
        {
            if (overwrite)
                clearReadOnly(destination);
            File.Move(source, destination, overwrite);
        });
    }

    public static void DeleteFile(string path)
    {
        Run(() =>
        {
            clearReadOnly(path);
            File.Delete(path);
        });
    }

    public static void MoveDirectory(string source, string destination)
    {
        Run(() => Directory.Move(source, destination));
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
