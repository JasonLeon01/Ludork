using Avalonia;
using System;
using Ludork.Services;

namespace Ludork;

sealed class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
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
