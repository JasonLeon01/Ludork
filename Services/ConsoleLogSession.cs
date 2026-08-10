using System;
using System.Globalization;
using System.IO;
using System.Text;

namespace Ludork.Services;

internal sealed class ConsoleLogSession : IDisposable
{
    private const int FlushLineCount = 64;
    private readonly object syncRoot = new();
    private StreamWriter? writer;
    private int unflushedLines;

    public string? Start(string projectPath)
    {
        lock (syncRoot)
        {
            string? stopError = stopLocked();
            if (stopError is not null)
                return stopError;
            try
            {
                string logDirectory = Path.Combine(projectPath, "Log");
                Directory.CreateDirectory(logDirectory);
                string currentPath = Path.Combine(logDirectory, "Ludork.log");
                if (File.Exists(currentPath))
                    archiveCurrentLog(currentPath, logDirectory);
                FileStream stream = new(
                    currentPath,
                    FileMode.CreateNew,
                    FileAccess.Write,
                    FileShare.Read);
                writer = new StreamWriter(stream, new UTF8Encoding(false));
                unflushedLines = 0;
                return null;
            }
            catch (IOException exception)
            {
                writer = null;
                return exception.Message;
            }
            catch (UnauthorizedAccessException exception)
            {
                writer = null;
                return exception.Message;
            }
        }
    }

    public string? WriteLine(string line)
    {
        lock (syncRoot)
        {
            if (writer is null)
                return null;
            try
            {
                writer.WriteLine(line);
                unflushedLines++;
                if (unflushedLines >= FlushLineCount)
                {
                    writer.Flush();
                    unflushedLines = 0;
                }
                return null;
            }
            catch (IOException exception)
            {
                string? stopError = stopLocked();
                return stopError ?? exception.Message;
            }
        }
    }

    public string? Stop()
    {
        lock (syncRoot)
            return stopLocked();
    }

    public void Dispose()
    {
        Stop();
    }

    private static void archiveCurrentLog(string currentPath, string logDirectory)
    {
        DateTime modifiedAt = File.GetLastWriteTime(currentPath);
        string timestamp = modifiedAt.ToString("yyyy-MM-dd-HH-mm-ss", CultureInfo.InvariantCulture);
        string baseName = "Ludork-" + timestamp;
        string archivePath = Path.Combine(logDirectory, baseName + ".log");
        int suffix = 2;
        while (File.Exists(archivePath))
        {
            archivePath = Path.Combine(
                logDirectory,
                baseName + "-" + suffix.ToString(CultureInfo.InvariantCulture) + ".log");
            suffix++;
        }
        File.Move(currentPath, archivePath);
    }

    private string? stopLocked()
    {
        if (writer is null)
            return null;
        StreamWriter current = writer;
        writer = null;
        unflushedLines = 0;
        try
        {
            current.Dispose();
            return null;
        }
        catch (IOException exception)
        {
            return exception.Message;
        }
    }
}
