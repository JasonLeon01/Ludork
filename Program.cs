using Avalonia;
using System;
using System.IO;
using System.Text.Json;
using Ludork.Services;

namespace Ludork;

sealed class Program
{
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
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
}
