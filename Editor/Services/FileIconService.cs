using Avalonia.Media;
using System;

namespace Ludork.Services;

public sealed class FileIconService
{
    private readonly IFileIconBackend backend = FileIconBackend.Create();

    public IImage? getShellIcon(string path, bool isDirectory, int size)
        => backend.GetIcon(path, isDirectory, size);
}

internal interface IFileIconBackend
{
    IImage? GetIcon(string path, bool isDirectory, int size);
}

internal static class FileIconBackend
{
    public static IFileIconBackend Create()
    {
        if (OperatingSystem.IsWindows())
            return new WindowsFileIconBackend();
        if (OperatingSystem.IsMacOS())
            return new MacOSFileIconBackend();
        if (OperatingSystem.IsLinux())
            return new LinuxFileIconBackend();
        return new NullFileIconBackend();
    }
}

internal sealed class NullFileIconBackend : IFileIconBackend
{
    public IImage? GetIcon(string path, bool isDirectory, int size) => null;
}
