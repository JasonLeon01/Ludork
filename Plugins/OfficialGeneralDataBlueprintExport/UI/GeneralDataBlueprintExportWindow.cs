using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialGeneralDataBlueprintExport.UI;

internal sealed class GeneralDataBlueprintExportWindow : Window
{
    private readonly GeneralDataBlueprintExporter exporter;
    private readonly PluginLocalizer localizer;
    private readonly CancellationTokenSource lifetime;
    private readonly StackPanel rows = new() { Spacing = 4 };
    private readonly TextBlock status = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock issues = new() { TextWrapping = TextWrapping.Wrap };
    private readonly Button export = new() { IsEnabled = false, IsDefault = true };
    private bool busy;

    public GeneralDataBlueprintExportWindow(GeneralDataBlueprintExporter exporter,
        PluginLocalizer localizer, CancellationToken cancellationToken)
    {
        this.exporter = exporter;
        this.localizer = localizer;
        lifetime = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        Title = localizer.Text("windowTitle");
        Width = 760;
        Height = 430;
        MinWidth = 560;
        MinHeight = 320;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = PluginTheme.Brush("Background");
        FontFamily = PluginTheme.FontFamily;
        Content = createLayout();
        Opened += async (_, _) => await runAsync(scanAsync, "scanning");
        Closing += (_, args) =>
        {
            if (busy)
            {
                args.Cancel = true;
                lifetime.Cancel();
            }
        };
        Closed += (_, _) => lifetime.Dispose();
    }

    private Grid createLayout()
    {
        Grid layout = new() { Margin = new Thickness(16), RowDefinitions = new RowDefinitions("Auto,*,Auto,Auto,Auto"), RowSpacing = 8 };
        Grid header = new()
        {
            Margin = new Thickness(8, 0),
            ColumnDefinitions = new ColumnDefinitions(GeneralDataBlueprintTypeRow.Columns),
            ColumnSpacing = 8,
        };
        TextBlock typeLabel = new() { Text = localizer.Text("type") };
        Grid.SetColumn(typeLabel, 1);
        header.Children.Add(typeLabel);
        TextBlock pathLabel = new() { Text = localizer.Text("directory") };
        Grid.SetColumn(pathLabel, 2);
        header.Children.Add(pathLabel);
        layout.Children.Add(header);
        ScrollViewer list = new() { Content = rows, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
        Grid.SetRow(list, 1);
        layout.Children.Add(list);
        Grid.SetRow(status, 2);
        layout.Children.Add(status);
        issues.Foreground = PluginTheme.Brush("Warning");
        ScrollViewer details = new() { Content = issues, MaxHeight = 110, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
        Grid.SetRow(details, 3);
        layout.Children.Add(details);

        export.Content = localizer.Text("export");
        export.Click += async (_, _) => await runAsync(exportAsync, "exporting");
        Button close = new() { Content = localizer.Text("close"), IsCancel = true };
        close.Click += (_, _) => Close();
        StackPanel actions = new() { Orientation = Orientation.Horizontal, HorizontalAlignment = HorizontalAlignment.Right, Spacing = 8 };
        actions.Children.Add(export);
        actions.Children.Add(close);
        Grid.SetRow(actions, 4);
        layout.Children.Add(actions);
        return layout;
    }

    private async Task scanAsync()
    {
        GeneralDataBlueprintScan scan = await exporter.ScanAsync(lifetime.Token);
        foreach (string type in scan.Types)
        {
            GeneralDataBlueprintTypeRow row = new(type, localizer);
            row.SelectionChanged += (_, _) => updateExport();
            row.BrowseRequested += async (_, _) => await runAsync(() => browseAsync(row));
            rows.Children.Add(row);
        }
        status.Text = scan.Types.Count == 0 ? localizer.Text("noTypes") : string.Empty;
        issues.Text = string.Join(Environment.NewLine, scan.Issues.Select(issue => FormatIssue(localizer, issue)));
    }

    private async Task exportAsync()
    {
        Dictionary<string, string> directories = rows.Children.OfType<GeneralDataBlueprintTypeRow>()
            .Where(row => row.IsSelected).ToDictionary(row => row.TypeKey, row => row.Directory, StringComparer.Ordinal);
        GeneralDataBlueprintExportResult result = await exporter.ExportAsync(directories, lifetime.Token);
        status.Text = localizer.Format("summary", result.Created, result.Existing, result.Conflicts);
        issues.Text = string.Join(Environment.NewLine, result.Issues.Select(issue => FormatIssue(localizer, issue)));
    }

    internal static string FormatIssue(PluginLocalizer localizer, GeneralDataBlueprintIssue issue)
        => localizer.Format("issue." + issue.Kind, issue.Arguments);

    private async Task browseAsync(GeneralDataBlueprintTypeRow row)
    {
        string root = Path.Combine(exporter.ProjectPath, "Data", "Blueprints");
        using IStorageFolder? start = await StorageProvider.TryGetFolderFromPathAsync(root);
        IReadOnlyList<IStorageFolder> selected = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
        {
            Title = localizer.Text("folderTitle"),
            SuggestedStartLocation = start,
            AllowMultiple = false,
        });
        if (selected.Count == 0)
            return;
        using IStorageFolder folder = selected[0];
        string? path = folder.TryGetLocalPath();
        string? relative = path is null ? null : Path.GetRelativePath(root, path).Replace('\\', '/');
        if (relative is null || Path.IsPathRooted(relative) || relative == ".." || relative.StartsWith("../", StringComparison.Ordinal))
        {
            status.Text = localizer.Text("invalidFolder");
            return;
        }
        row.Directory = relative == "." ? "Data/Blueprints" : $"Data/Blueprints/{relative}";
        if (status.Text == localizer.Text("invalidFolder"))
            status.Text = string.Empty;
    }

    private async Task runAsync(Func<Task> action, string? message = null)
    {
        if (busy)
            return;
        busy = true;
        rows.IsEnabled = false;
        export.IsEnabled = false;
        if (message is not null)
            status.Text = localizer.Text(message);
        try
        {
            await action();
        }
        catch (OperationCanceledException) when (lifetime.IsCancellationRequested)
        {
        }
        catch (Exception exception)
        {
            status.Text = localizer.Text("failed");
            issues.Text = exception is PluginWorkspaceUnavailableException
                ? localizer.Text("unavailable") : exception.Message;
        }
        finally
        {
            busy = false;
            rows.IsEnabled = true;
            updateExport();
            if (lifetime.IsCancellationRequested)
                Close();
        }
    }

    private void updateExport() => export.IsEnabled = !busy
        && rows.Children.OfType<GeneralDataBlueprintTypeRow>().Any(row => row.IsSelected);
}
