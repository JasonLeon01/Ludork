using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Ludork.Services;
using System;
using System.Threading.Tasks;
using System.Threading;
using Ludork.Views.Utils;

namespace Ludork.Views;

public partial class ProjectCompilerDialog : Window
{
    private bool opened;
    private bool confirming;
    private bool closed;
    private bool closingDownload;
    private readonly Func<Window, string?, Task<bool>>? applyCompiler;

    public ProjectCompilerDialog() : this(null, null)
    {
    }

    public ProjectCompilerDialog(string? projectPath, string? compiler,
        Func<Window, string?, Task<bool>>? applyCompiler = null)
    {
        this.applyCompiler = applyCompiler;
        InitializeComponent();
        Title = LocaleService.Get("COMPILER_CHANGE");
        CancelButton.Content = LocaleService.Get("CANCEL");
        ConfirmButton.Content = LocaleService.Get("CONFIRM");
        ConfirmButton.IsEnabled = false;
        CompilerSelection.StateChanged += (_, _) => updateConfirmState();
        Opened += async (_, _) =>
        {
            opened = true;
            await CompilerSelection.RefreshAsync(projectPath, compiler);
        };
        Activated += async (_, _) =>
        {
            if (opened && !confirming && !closingDownload && !CompilerSelection.IsBusy)
                await CompilerSelection.RefreshAsync(projectPath,
                    CompilerSelection.HasSelection ? CompilerSelection.SelectedCompilerPath : compiler);
        };
        Closing += onClosing;
        Closed += (_, _) =>
        {
            closed = true;
            CompilerSelection.CancelPendingOperations();
        };
        KeyDown += (_, args) =>
        {
            if (args.Key == Key.Escape && !confirming && !closingDownload)
            {
                args.Handled = true;
                Close(false);
            }
        };
    }

    public static Task<bool> ShowAsync(Window owner, string projectPath, string? compiler,
        Func<Window, string?, Task<bool>> applyCompiler) =>
        new ProjectCompilerDialog(projectPath, compiler, applyCompiler).ShowDialog<bool>(owner);

    internal static Task<bool> ConfirmChangeAsync(Window owner, string? previous, string? target,
        CancellationToken token) => ConfirmationDialog.ShowAsync(owner, LocaleService.Get("COMPILER_SWITCH_TITLE"),
            string.Format(LocaleService.Get("COMPILER_SWITCH_MESSAGE"), previous ?? "MSVC", target ?? "MSVC"), token,
            confirmLabel: LocaleService.Get("COMPILER_SWITCH_CONFIRM"));

    private void updateConfirmState() => ConfirmButton.IsEnabled = !confirming && !closingDownload
        && CompilerSelection.HasSelection && !CompilerSelection.IsBusy;

    private async void onClosing(object? sender, WindowClosingEventArgs args)
    {
        if (confirming || closingDownload)
        {
            args.Cancel = true;
            return;
        }
        if (!CompilerSelection.IsDownloading)
            return;
        args.Cancel = true;
        closingDownload = true;
        IsEnabled = false;
        await CompilerSelection.StopAsync();
        closingDownload = false;
        Close(false);
    }

    private async void onConfirm(object? sender, RoutedEventArgs args)
    {
        if (!ConfirmButton.IsEnabled || applyCompiler is null)
            return;
        confirming = true;
        CompilerSelection.IsEnabled = false;
        CancelButton.IsEnabled = false;
        updateConfirmState();
        try
        {
            if (await applyCompiler(this, CompilerSelection.SelectedCompilerPath))
            {
                confirming = false;
                Close(true);
            }
        }
        finally
        {
            if (!closed)
            {
                confirming = false;
                CompilerSelection.IsEnabled = true;
                CancelButton.IsEnabled = true;
                updateConfirmState();
            }
        }
    }

    private void onCancel(object? sender, RoutedEventArgs args)
    {
        if (!confirming && !closingDownload)
            Close(false);
    }
}
