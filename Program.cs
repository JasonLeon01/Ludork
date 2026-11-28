using Avalonia;
using System;
using System.IO;
using System.Text.Json;
using Avalonia.Threading;
using Ludork.Services;

namespace Ludork;

sealed class Program
{
    private static EditorLaunchService? launches;

    [STAThread]
    public static void Main(string[] args)
    {
        if (args.Length > 0 && string.Equals(args[0], "--check-build-info", StringComparison.Ordinal))
        {
            try
            {
                if (args.Length != 2)
                    throw new ArgumentException("Usage: Ludork --check-build-info <metadata-file>");
                EditorVersionService.ValidateBuildInfo(args[1]);
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException
                or JsonException or InvalidOperationException or ArgumentException)
            {
                Console.Error.WriteLine(exception.Message);
                Environment.ExitCode = 1;
            }
            return;
        }
        if (args.Length > 0 && string.Equals(args[0], "--compile-locale", StringComparison.OrdinalIgnoreCase))
        {
            LocaleService.Compile();
            return;
        }
        if (args.Length > 0 && string.Equals(args[0], "--notify-shell-association-changed", StringComparison.OrdinalIgnoreCase))
        {
            ShellAssociationService.NotifyChanged();
            return;
        }
        string? argument = Array.Find(args, value => value.EndsWith(".proj", StringComparison.OrdinalIgnoreCase));
        string? projectPath = argument is null ? null : Path.GetFullPath(argument);
        using EditorLaunchService instance = new();
        if (!instance.AcquireOrForward(projectPath))
            return;
        launches = instance;
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(projectPath is null ? [] : [projectPath]);
    }

    internal static void ListenForLaunches(Func<string?, bool> activate)
        => launches?.Listen(async (path, token) => await Dispatcher.UIThread.InvokeAsync(
            () => activate(path), DispatcherPriority.Normal, token));

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
}
