using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using System;
using System.Linq;

namespace Ludork.Views;

public partial class PackLogDialog : Window
{
    private bool closeEnabled;
    private ScrollViewer? logScrollViewer;

    public PackLogDialog()
    {
        InitializeComponent();
        Title = LocaleService.Get("PACK_TITLE");
        CloseButton.Content = LocaleService.Get("CLOSE");
        Closing += onClosing;
        Opened += (_, _) =>
            logScrollViewer = LogArea.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
    }

    public void AppendLog(string text)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.InvokeAsync(() => AppendLog(text)).GetAwaiter().GetResult();
            return;
        }
        LogArea.Text += text;
        LogArea.CaretIndex = LogArea.Text?.Length ?? 0;
        Dispatcher.UIThread.Post(() => logScrollViewer?.ScrollToEnd(), DispatcherPriority.Background);
    }

    public void Finish(ProjectPackResult result)
    {
        if (!Dispatcher.UIThread.CheckAccess())
        {
            Dispatcher.UIThread.Post(() => Finish(result));
            return;
        }

        if (result.Success)
        {
            AppendLog(Environment.NewLine + LocaleService.Get("PACK_SUCCESS") + Environment.NewLine);
        }
        else if (result.Failure == ProjectPackFailure.Cancelled)
        {
            AppendLog(Environment.NewLine + LocaleService.Get("PACK_CANCELLED") + Environment.NewLine);
        }
        else
        {
            string detail = result.Failure switch
            {
                ProjectPackFailure.ProjectInvalid => LocaleService.Get("PACK_NO_PROJECT") + Environment.NewLine + result.Detail,
                ProjectPackFailure.AppNameUnchanged => LocaleService.Get("PACK_APP_NAME_UNCHANGED"),
                ProjectPackFailure.PluginPreparationFailed => LocaleService.Get("PACK_PLUGIN_PREPARATION_FAILED") + Environment.NewLine + result.Detail,
                ProjectPackFailure.PlatformUnsupported => LocaleService.Get("PACK_PLATFORM_NOT_IMPLEMENTED") + Environment.NewLine + result.Detail,
                ProjectPackFailure.ScriptMissing => LocaleService.Get("PACK_SCRIPT_MISSING") + Environment.NewLine + result.Detail,
                ProjectPackFailure.LaunchFailed => LocaleService.Get("PACK_LAUNCH_FAILED") + Environment.NewLine + result.Detail,
                ProjectPackFailure.ToolchainUnavailable => LocaleService.Get("PACK_IOS_TOOLCHAIN_UNAVAILABLE"),
                ProjectPackFailure.DeviceUnavailable => LocaleService.Get("PACK_IOS_DEVICE_UNAVAILABLE"),
                ProjectPackFailure.SigningUnavailable => LocaleService.Get("PACK_IOS_SIGNING_UNAVAILABLE"),
                ProjectPackFailure.IOSProjectUnsupported => LocaleService.Get("PACK_IOS_PROJECT_UNSUPPORTED"),
                ProjectPackFailure.HarmonyToolchainUnavailable => LocaleService.Get("PACK_HARMONY_TOOLCHAIN_UNAVAILABLE"),
                ProjectPackFailure.HarmonyDeviceUnavailable => LocaleService.Get("PACK_HARMONY_DEVICE_UNAVAILABLE"),
                ProjectPackFailure.HarmonySigningUnavailable => LocaleService.Get("PACK_HARMONY_SIGNING_UNAVAILABLE"),
                ProjectPackFailure.HarmonyProjectUnsupported => LocaleService.Get("PACK_HARMONY_PROJECT_UNSUPPORTED"),
                ProjectPackFailure.AndroidToolchainUnavailable => LocaleService.Get("PACK_ANDROID_TOOLCHAIN_UNAVAILABLE"),
                ProjectPackFailure.AndroidSigningUnavailable => LocaleService.Get("PACK_ANDROID_SIGNING_UNAVAILABLE"),
                ProjectPackFailure.AndroidProjectUnsupported => LocaleService.Get("PACK_ANDROID_PROJECT_UNSUPPORTED"),
                ProjectPackFailure.PackFailed => string.Format(LocaleService.Get("PACK_EXIT_CODE"), result.Detail),
                _ => result.Detail,
            };
            AppendLog(Environment.NewLine + detail + Environment.NewLine);
            AppendLog(Environment.NewLine + LocaleService.Get("PACK_FAILED") + Environment.NewLine);
        }

        closeEnabled = true;
        CloseButton.IsEnabled = true;
    }

    private void onClosing(object? sender, WindowClosingEventArgs args)
    {
        if (!closeEnabled)
            args.Cancel = true;
    }

    private void onClose(object? sender, RoutedEventArgs args)
    {
        if (closeEnabled)
            Close();
    }
}
