using Avalonia.Platform.Storage;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Services;

public static class XcodeToolchainService
{
    private static readonly Uri appStoreUri = new("macappstores://apps.apple.com/app/id497799835?mt=12");

    public static async Task<bool> IsInstalledAsync(CancellationToken cancellationToken = default)
    {
        cancellationToken.ThrowIfCancellationRequested();
        if (!OperatingSystem.IsMacOS())
            return false;
        if (hasToolchain(Environment.GetEnvironmentVariable("DEVELOPER_DIR")))
            return true;
        string? selected = await readSelectedDirectoryAsync(cancellationToken).ConfigureAwait(false);
        cancellationToken.ThrowIfCancellationRequested();
        return hasToolchain(selected)
            || hasToolchain("/Applications/Xcode.app/Contents/Developer")
            || hasToolchain(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
                "Applications", "Xcode.app", "Contents", "Developer"));
    }

    public static async Task<bool> OpenAppStoreAsync(ILauncher launcher)
    {
        try
        {
            return await launcher.LaunchUriAsync(appStoreUri);
        }
        catch (Exception exception) when (exception is Win32Exception or IOException
            or InvalidOperationException or NotSupportedException)
        {
            return false;
        }
    }

    private static bool hasToolchain(string? path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return false;
        path = path.Trim();
        if (path.StartsWith("~/", StringComparison.Ordinal))
            path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), path[2..]);
        if (!Path.IsPathFullyQualified(path))
            return false;
        string developerDirectory = Path.Combine(path, "Contents", "Developer");
        if (Directory.Exists(developerDirectory))
            path = developerDirectory;
        return File.Exists(Path.Combine(path, "usr", "bin", "xcodebuild"))
            && File.Exists(Path.Combine(path, "Toolchains", "XcodeDefault.xctoolchain", "usr", "bin", "clang"))
            && Directory.Exists(Path.Combine(path, "Platforms", "MacOSX.platform", "Developer", "SDKs"));
    }

    private static async Task<string?> readSelectedDirectoryAsync(CancellationToken cancellationToken)
    {
        ProcessStartInfo startInfo = new()
        {
            FileName = "/usr/bin/xcode-select",
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        startInfo.ArgumentList.Add("-p");
        try
        {
            ToolProcessResult result = await ToolProcessRunner.CaptureAsync(
                startInfo, cancellationToken, TimeSpan.FromSeconds(5)).ConfigureAwait(false);
            return result.ExitCode == 0 ? result.StandardOutput.Trim() : null;
        }
        catch (TimeoutException)
        {
            return null;
        }
        catch (Exception exception) when (exception is Win32Exception or IOException or InvalidOperationException)
        {
            return null;
        }
    }
}
