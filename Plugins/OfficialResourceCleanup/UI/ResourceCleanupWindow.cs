using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialResourceCleanup.Localization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialResourceCleanup.UI;

internal sealed partial class ResourceCleanupWindow : Window
{
    private readonly IResourceCleanupHost host;
    private readonly IReadOnlyList<string> nativeKeepPaths;
    private readonly PluginLocalizer localizer;
    private readonly CancellationTokenSource lifetime;
    private CancellationTokenSource? operation;
    private ResourceCleanupReport? report;
    private bool initialized;
    private bool closeRequested;
    private bool updatingKeepPaths;
    private string savedKeepText = string.Empty;

    public ResourceCleanupWindow(
        IResourceCleanupHost host,
        IReadOnlyList<string> nativeKeepPaths,
        PluginLocalizer localizer,
        CancellationToken cancellationToken)
    {
        this.host = host;
        this.nativeKeepPaths = nativeKeepPaths;
        this.localizer = localizer;
        lifetime = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        Title = localizer.Text("windowTitle");
        Width = 1120;
        Height = 830;
        MinWidth = 850;
        MinHeight = 700;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = PluginTheme.Brush("Background");
        FontFamily = PluginTheme.FontFamily;
        configureControls();
        Content = createLayout();
        updateActions();
        Opened += async (_, _) => await runAsync("loading", async (token, _) =>
        {
            IReadOnlyList<string> paths = await host.ReadKeepPathsAsync(token);
            replaceKeepPaths(paths);
            savedKeepText = normalizeKeepText();
            initialized = true;
            statusText.Text = localizer.Text("ready");
        });
        Closing += async (_, args) =>
        {
            if (operation is not null)
            {
                args.Cancel = true;
                closeRequested = true;
                operation.Cancel();
                return;
            }
            if (!initialized || normalizeKeepText() == savedKeepText)
                return;
            args.Cancel = true;
            closeRequested = false;
            await runAsync("saving", async (token, _) =>
            {
                await saveKeepPathsAsync(token);
                closeRequested = true;
            });
        };
        Closed += (_, _) =>
        {
            lifetime.Cancel();
            lifetime.Dispose();
        };
    }

    private void configureControls()
    {
        keepInput.AcceptsReturn = true;
        keepInput.Height = double.NaN;
        keepInput.MinHeight = 110;
        keepInput.VerticalAlignment = VerticalAlignment.Stretch;
        keepInput.VerticalContentAlignment = VerticalAlignment.Top;
        keepInput.TextWrapping = TextWrapping.NoWrap;
        keepInput.PlaceholderText = localizer.Text("keepExample");
        keepInput.TextChanged += (_, _) =>
        {
            if (updatingKeepPaths)
                return;
            if (report is not null)
                statusText.Text = localizer.Text("reportStale");
            report = null;
            updateActions();
        };
        saveKeepButton.Content = localizer.Text("saveKeep");
        saveKeepButton.Click += async (_, _) => await runAsync("saving", async (token, _) =>
        {
            await saveKeepPathsAsync(token);
            statusText.Text = localizer.Text("keepSaved");
        });
        scanButton.Content = localizer.Text("scan");
        scanButton.Click += async (_, _) => await scanAsync();
        keepSelectedButton.Content = localizer.Text("keepSelected");
        keepSelectedButton.Click += async (_, _) =>
        {
            string[] selected = dataList.SelectedItems!.OfType<ResourceCleanupCandidate>()
                .Concat(assetList.SelectedItems!.OfType<ResourceCleanupCandidate>())
                .Select(candidate => candidate.RelativePath).ToArray();
            if (selected.Length == 0)
                return;
            replaceKeepPaths(readKeepPaths().Concat(selected).Distinct(StringComparer.Ordinal).ToArray());
            report = null;
            await scanAsync();
        };
        trashButton.Content = localizer.Text("trash");
        trashButton.Click += async (_, _) => await trashAsync();
        cancelButton.Content = localizer.Text("cancel");
        cancelButton.Click += (_, _) => operation?.Cancel();
        closeButton.Content = localizer.Text("close");
        closeButton.Click += (_, _) => Close();
        dataList.SelectionChanged += (_, _) => updateActions();
        assetList.SelectionChanged += (_, _) => updateActions();
    }

    private async Task scanAsync()
    {
        await runAsync("scanning", async (token, progress) =>
        {
            report = null;
            dataList.ItemsSource = null;
            assetList.ItemsSource = null;
            issueList.ItemsSource = null;
            outcomeList.ItemsSource = null;
            outcomeTab.IsVisible = false;
            issueTab.IsVisible = false;
            dataTab.Header = "Data";
            assetTab.Header = "Assets";
            await saveKeepPathsAsync(token);
            ResourceCleanupReport scanned = await host.ScanAsync(nativeKeepPaths, readKeepPaths(), progress, token);
            token.ThrowIfCancellationRequested();
            report = scanned;
            ResourceCleanupCandidate[] data = scanned.Candidates.Where(item => item.Category == "Data").ToArray();
            ResourceCleanupCandidate[] assets = scanned.Candidates.Where(item => item.Category == "Assets").ToArray();
            dataList.ItemsSource = data;
            assetList.ItemsSource = assets;
            dataTab.Header = localizer.Format("categorySummary", "Data", data.Length, localizer.Size(data.Sum(item => item.SizeBytes)));
            assetTab.Header = localizer.Format("categorySummary", "Assets", assets.Length, localizer.Size(assets.Sum(item => item.SizeBytes)));
            issueList.ItemsSource = scanned.Issues;
            issueTab.IsVisible = scanned.Issues.Count != 0;
            issueTab.Header = localizer.Format("issues", scanned.Issues.Count);
            resultTabs.SelectedItem = issueTab.IsVisible ? issueTab : data.Length != 0 ? dataTab : assetTab;
            statusText.Text = scanned.Issues.Count != 0
                ? localizer.Text("scanBlocked")
                : scanned.Candidates.Count == 0 ? localizer.Text("noCandidates")
                : localizer.Format("scanSummary", scanned.Candidates.Count, localizer.Size(scanned.TotalBytes));
        });
    }

    private async Task trashAsync()
    {
        ResourceCleanupReport? current = report;
        if (current?.CanTrash != true
            || !await ResourceCleanupConfirmation.ShowAsync(this, current, localizer))
            return;
        await runAsync("recycling", async (token, progress) =>
        {
            report = null;
            ResourceCleanupTrashResult result = await host.TrashAsync(current.Id, progress, token);
            outcomeList.ItemsSource = result.RecycledPaths
                .Select(path => new ResourceCleanupIssue(path, localizer.Text("recycled")))
                .Concat(result.RemainingPaths.Select((path, index) => new ResourceCleanupIssue(path,
                    index == 0 && !result.Cancelled && result.Error.Length != 0
                        ? localizer.Text("failed") : localizer.Text("remaining"))))
                .ToArray();
            outcomeTab.IsVisible = true;
            resultTabs.SelectedItem = outcomeTab;
            statusText.Text = localizer.Format("trashSummary", result.RecycledPaths.Count, result.RemainingPaths.Count)
                + (result.Cancelled ? " " + localizer.Text("cancelled") : string.Empty)
                + (result.Error.Length != 0 ? Environment.NewLine + result.Error : string.Empty);
        });
    }

    private async Task saveKeepPathsAsync(CancellationToken token)
    {
        string[] paths = readKeepPaths();
        await host.SaveKeepPathsAsync(paths, token);
        savedKeepText = string.Join('\n', paths);
    }

    private string[] readKeepPaths() => (keepInput.Text ?? string.Empty)
        .Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
        .Distinct(StringComparer.Ordinal).ToArray();

    private string normalizeKeepText() => string.Join('\n', readKeepPaths());

    private void replaceKeepPaths(IEnumerable<string> paths)
    {
        updatingKeepPaths = true;
        keepInput.Text = string.Join(Environment.NewLine, paths);
        updatingKeepPaths = false;
    }

    private async Task runAsync(
        string status,
        Func<CancellationToken, IProgress<ResourceCleanupProgress>, Task> action)
    {
        if (operation is not null)
            return;
        using CancellationTokenSource current = CancellationTokenSource.CreateLinkedTokenSource(lifetime.Token);
        operation = current;
        using ResourceCleanupProgressSink progress = new(showProgress);
        statusText.Text = localizer.Text(status);
        progressText.Text = string.Empty;
        currentPathText.Text = string.Empty;
        progressBar.IsIndeterminate = true;
        updateActions();
        try
        {
            await action(current.Token, progress);
        }
        catch (OperationCanceledException) when (current.IsCancellationRequested)
        {
            statusText.Text = localizer.Text("cancelled");
        }
        catch (Exception exception)
        {
            statusText.Text = localizer.Text("operationFailed") + Environment.NewLine + exception.Message;
        }
        finally
        {
            operation = null;
            updateActions();
            if (closeRequested)
                Close();
        }
    }

    private void showProgress(ResourceCleanupProgress value)
    {
        progressText.Text = localizer.Format("progress", localizer.Text("stage" + value.Stage), value.Completed, value.Total);
        currentPathText.Text = value.CurrentPath;
        ToolTip.SetTip(currentPathText, value.CurrentPath);
        progressBar.IsIndeterminate = value.Total <= 0;
        progressBar.Maximum = Math.Max(1, value.Total);
        progressBar.Value = Math.Clamp(value.Completed, 0, Math.Max(1, value.Total));
    }

    private void updateActions()
    {
        bool idle = operation is null && initialized;
        keepInput.IsEnabled = idle;
        scanButton.IsEnabled = idle;
        saveKeepButton.IsEnabled = idle && normalizeKeepText() != savedKeepText;
        keepSelectedButton.IsEnabled = idle
            && (dataList.SelectedItems?.Count > 0 || assetList.SelectedItems?.Count > 0);
        trashButton.IsEnabled = idle && report?.CanTrash == true;
        cancelButton.IsVisible = operation is not null;
        progressPanel.IsVisible = operation is not null;
        dataList.IsEnabled = idle;
        assetList.IsEnabled = idle;
    }
}
