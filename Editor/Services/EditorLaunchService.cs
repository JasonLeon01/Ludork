using Microsoft.Win32.SafeHandles;
using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

internal sealed class EditorLaunchService : IDisposable
{
    private readonly string pipeName;
    private readonly Mutex? owner;
    private readonly CancellationTokenSource stopping = new();
    private SafeFileHandle? lockFile;
    private bool acquired;
    private Task? listener;

    public EditorLaunchService()
    {
        string installation = FileSystemPathIdentity.GetKey(AppContext.BaseDirectory);
        string identity = Environment.UserName + "\n" + installation;
        if (OperatingSystem.IsWindows())
        {
            using WindowsIdentity user = WindowsIdentity.GetCurrent();
            using Process currentProcess = Process.GetCurrentProcess();
            identity = user.User!.Value + "\n" + currentProcess.SessionId + "\n" + installation;
        }
        string name = "Ludork-" + Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(identity)))[..24];
        if (OperatingSystem.IsMacOS())
        {
            const int darwinUserTempDirectory = 65537;
            StringBuilder directory = new(1024);
            nuint size = confstr(darwinUserTempDirectory, directory, (nuint)directory.Capacity);
            if (size == 0 || size > (nuint)directory.Capacity)
                throw new IOException("The macOS user temporary directory is unavailable.");
            pipeName = Path.Combine(directory.ToString(), name);
        }
        else
        {
            pipeName = name;
            owner = new Mutex(false, OperatingSystem.IsWindows() ? name : "Global\\" + name);
        }
    }

    public bool AcquireOrForward(string? projectPath)
    {
        while (true)
        {
            acquired = tryAcquire();
            if (acquired)
                return true;
            using NamedPipeClientStream pipe = new(".", pipeName, PipeDirection.InOut, PipeOptions.CurrentUserOnly);
            try
            {
                pipe.Connect(250);
                if (OperatingSystem.IsWindows() && GetNamedPipeServerProcessId(pipe.SafePipeHandle, out uint processId))
                    AllowSetForegroundWindow(processId);
                using StreamWriter writer = new(pipe, leaveOpen: true);
                writer.WriteLine(JsonSerializer.Serialize(projectPath));
                writer.Flush();
                if (pipe.ReadByte() == 1)
                    return false;
            }
            catch (TimeoutException)
            {
                continue;
            }
            catch (IOException)
            {
            }
            Thread.Sleep(100);
        }
    }

    private bool tryAcquire()
    {
        if (owner is not null)
        {
            try
            {
                return owner.WaitOne(0);
            }
            catch (AbandonedMutexException)
            {
                return true;
            }
        }
        const int darwinWouldBlock = 35;
        try
        {
            lockFile = File.OpenHandle(pipeName + ".lock", FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None);
            return true;
        }
        catch (IOException exception) when (exception.HResult == darwinWouldBlock)
        {
            return false;
        }
    }

    public void Listen(Func<string?, CancellationToken, Task<bool>> activate)
    {
        listener = Task.Run(() => listenAsync(activate, stopping.Token));
    }

    private async Task listenAsync(Func<string?, CancellationToken, Task<bool>> activate, CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            using NamedPipeServerStream pipe = new(pipeName, PipeDirection.InOut, 1, PipeTransmissionMode.Byte,
                PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
            await pipe.WaitForConnectionAsync(token);
            try
            {
                using StreamReader reader = new(pipe, leaveOpen: true);
                string? line = await reader.ReadLineAsync(token);
                if (line is null)
                    continue;
                string? projectPath = JsonSerializer.Deserialize<string?>(line);
                bool accepted = await activate(projectPath, token).WaitAsync(token);
                await pipe.WriteAsync(new byte[] { accepted ? (byte)1 : (byte)0 }, token);
            }
            catch (JsonException exception)
            {
                Trace.WriteLine(exception);
            }
            catch (IOException exception)
            {
                Trace.WriteLine(exception);
            }
        }
    }

    public void Dispose()
    {
        stopping.Cancel();
        try
        {
            listener?.GetAwaiter().GetResult();
        }
        catch (OperationCanceledException) when (stopping.IsCancellationRequested)
        {
        }
        finally
        {
            stopping.Dispose();
            lockFile?.Dispose();
            if (acquired && owner is not null)
                owner.ReleaseMutex();
            owner?.Dispose();
        }
    }

    [DllImport("/usr/lib/libSystem.B.dylib", ExactSpelling = true)]
    private static extern nuint confstr(int name, StringBuilder buffer, nuint length);

    [DllImport("kernel32.dll")]
    private static extern bool GetNamedPipeServerProcessId(SafePipeHandle pipe, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool AllowSetForegroundWindow(uint processId);
}
