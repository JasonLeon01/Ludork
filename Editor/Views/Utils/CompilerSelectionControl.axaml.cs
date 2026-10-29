using Avalonia.Controls;
using Avalonia.Interactivity;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Views.Utils;

public partial class CompilerSelectionControl : UserControl
{
    private CancellationTokenSource? operationCancellation;
    private TaskCompletionSource? downloadCompletion;
    private IReadOnlyList<WindowsCompiler> compilers = [];
    private string? projectPath;
    private int generation;
    private bool downloading;
    private bool stopped;
    private bool showDownload = true;

    public CompilerSelectionControl()
    {
        InitializeComponent();
        CompilerLabel.Text = LocaleService.Get("COMPILER_LABEL");
        CompilerBox.SelectionChanged += (_, _) => updateState();
        updateState();
    }

    public event EventHandler? StateChanged;
    public WindowsCompiler? SelectedCompiler => (CompilerBox.SelectedItem as ComboBoxItem)?.Tag as WindowsCompiler;
    public string? SelectedCompilerPath => SelectedCompiler?.Compiler;
    public bool HasSelection => SelectedCompiler is not null;
    public bool IsBusy { get; private set; }
    public bool IsDownloading => downloading;
    public bool ShowDownload
    {
        get => showDownload;
        set
        {
            showDownload = value;
            DownloadButton.IsVisible = value;
        }
    }

    public async Task RefreshAsync(string? projectPath = null, string? preferredCompiler = null,
        CancellationToken cancellationToken = default, bool selectDefault = false)
    {
        if (stopped || downloading)
            return;
        this.projectPath = projectPath;
        int currentGeneration = ++generation;
        operationCancellation?.Cancel();
        using CancellationTokenSource cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        operationCancellation = cancellation;
        IsBusy = true;
        ErrorText.IsVisible = false;
        updateState();
        try
        {
            IReadOnlyList<WindowsCompiler> found = await WindowsCompilerService.ListAsync(projectPath, cancellation.Token);
            if (stopped || currentGeneration != generation)
                return;
            compilers = found;
            CompilerBox.Items.Clear();
            foreach (WindowsCompiler compiler in found)
            {
                ComboBoxItem item = new()
                {
                    Content = displayName(compiler),
                    Tag = compiler,
                };
                ToolTip.SetTip(item, compiler.Compiler ?? compiler.Name);
                CompilerBox.Items.Add(item);
            }
            int selected = -1;
            for (int index = 0; index < found.Count; index++)
            {
                if (sameCompiler(found[index].Compiler, preferredCompiler))
                {
                    selected = index;
                    break;
                }
            }
            CompilerBox.SelectedIndex = selected >= 0 ? selected : selectDefault && found.Count > 0 ? 0 : -1;
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception exception) when (isOperationException(exception))
        {
            if (!stopped && currentGeneration == generation)
            {
                compilers = [];
                CompilerBox.Items.Clear();
                showError(exception.Message);
            }
        }
        finally
        {
            if (ReferenceEquals(operationCancellation, cancellation))
                operationCancellation = null;
            if (!stopped && currentGeneration == generation)
            {
                IsBusy = false;
                updateState();
            }
        }
    }

    public void CancelPendingOperations()
    {
        stopped = true;
        ++generation;
        operationCancellation?.Cancel();
    }

    public Task StopAsync()
    {
        Task completion = downloadCompletion?.Task ?? Task.CompletedTask;
        CancelPendingOperations();
        return completion;
    }

    public void SetUnavailable(string message)
    {
        ++generation;
        operationCancellation?.Cancel();
        compilers = [];
        CompilerBox.Items.Clear();
        IsBusy = false;
        showError(message);
        updateState();
    }

    private static bool sameCompiler(string? first, string? second) =>
        WindowsCompilerService.SameCompiler(first, second);

    private static string displayName(WindowsCompiler compiler)
    {
        if (compiler.Compiler is null)
            return compiler.Name;
        string source = compiler.Source.ToLowerInvariant() switch
        {
            "managed" => LocaleService.Get("COMPILER_SOURCE_MANAGED"),
            "path" => LocaleService.Get("COMPILER_SOURCE_PATH"),
            "project" => LocaleService.Get("COMPILER_SOURCE_PROJECT"),
            _ => compiler.Source,
        };
        return $"{compiler.Name} {compiler.Version} ({source})";
    }

    private void updateState()
    {
        bool installed = compilers.Any(compiler => sameCompiler(compiler.Compiler, WindowsCompilerService.ManagedCompilerPath));
        CompilerLabel.Text = LocaleService.Get(compilers.Count > 0 ? "COMPILER_LABEL"
            : IsBusy && !downloading ? "COMPILER_DETECTING" : "COMPILER_NOT_FOUND");
        CompilerBox.IsVisible = compilers.Count > 0;
        CompilerBox.IsEnabled = !IsBusy && compilers.Count > 0;
        DownloadButton.IsVisible = ShowDownload;
        DownloadButton.IsEnabled = downloading || !IsBusy && !installed;
        DownloadButton.Content = LocaleService.Get(downloading ? "COMPILER_CANCEL_DOWNLOAD"
            : installed ? "COMPILER_INSTALLED" : "COMPILER_DOWNLOAD");
        if (!downloading)
        {
            StatusText.Text = compilers.Count > 0 && IsBusy ? LocaleService.Get("COMPILER_DETECTING")
                : compilers.Count > 0 && !HasSelection ? LocaleService.Get("COMPILER_SELECT_AVAILABLE") : string.Empty;
            StatusText.IsVisible = StatusText.Text.Length != 0;
            DownloadProgress.IsVisible = IsBusy;
            DownloadProgress.IsIndeterminate = IsBusy;
        }
        ToolTip.SetTip(CompilerBox, SelectedCompilerPath);
        StateChanged?.Invoke(this, EventArgs.Empty);
    }

    private async void onDownload(object? sender, RoutedEventArgs args)
    {
        if (downloading)
        {
            operationCancellation?.Cancel();
            return;
        }
        if (IsBusy || stopped)
            return;
        bool hadSelection = HasSelection;
        string? preferred = SelectedCompilerPath;
        using CancellationTokenSource cancellation = new();
        TaskCompletionSource completion = new(TaskCreationOptions.RunContinuationsAsynchronously);
        downloadCompletion = completion;
        operationCancellation = cancellation;
        downloading = true;
        IsBusy = true;
        ErrorText.IsVisible = false;
        StatusText.IsVisible = true;
        StatusText.Text = LocaleService.Get("COMPILER_DOWNLOADING");
        DownloadProgress.IsVisible = true;
        DownloadProgress.IsIndeterminate = true;
        updateState();
        bool installed = false;
        try
        {
            await WindowsCompilerService.InstallAsync(new Progress<ProjectTemplateProgress>(updateProgress), cancellation.Token);
            installed = true;
        }
        catch (OperationCanceledException)
        {
        }
        catch (Exception exception) when (isOperationException(exception))
        {
            if (!stopped)
                showError(LocaleService.Get("COMPILER_DOWNLOAD_FAILED") + Environment.NewLine + exception.Message);
        }
        finally
        {
            try
            {
                operationCancellation = null;
                downloading = false;
                IsBusy = false;
                if (!stopped)
                    updateState();
            }
            finally
            {
                downloadCompletion = null;
                completion.TrySetResult();
            }
        }
        if (installed && !stopped)
            await RefreshAsync(projectPath, hadSelection ? preferred : WindowsCompilerService.ManagedCompilerPath);
    }

    private void updateProgress(ProjectTemplateProgress progress)
    {
        if (stopped || !downloading)
            return;
        string stage = LocaleService.Get(progress.Stage == ProjectTemplateStage.Extracting
            ? "COMPILER_EXTRACTING" : "COMPILER_DOWNLOADING");
        DownloadProgress.IsIndeterminate = progress.TotalBytes is not > 0;
        if (progress.TotalBytes is > 0)
        {
            double percent = Math.Clamp(progress.CompletedBytes * 100d / progress.TotalBytes.Value, 0, 100);
            DownloadProgress.Value = percent;
            StatusText.Text = string.Format(LocaleService.Get("NEW_PROJECT_PROGRESS"), stage,
                percent, progress.CompletedBytes / 1048576d, progress.TotalBytes.Value / 1048576d);
        }
        else
            StatusText.Text = string.Format(LocaleService.Get("NEW_PROJECT_PROGRESS_UNKNOWN"), stage,
                progress.CompletedBytes / 1048576d);
    }

    private void showError(string message)
    {
        ErrorText.Text = message;
        ErrorText.IsVisible = true;
    }

    private static bool isOperationException(Exception exception) => exception is IOException
        or InvalidDataException or UnauthorizedAccessException or HttpRequestException
        or InvalidOperationException or Win32Exception or JsonException or ArgumentException;
}
