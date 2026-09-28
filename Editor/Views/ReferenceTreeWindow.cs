using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;

namespace Ludork.Views;

public sealed class ReferenceTreeWindow : Window
{
    private readonly ReferenceIndexService referenceIndex;
    private readonly string nodeId;
    private readonly ReferenceTreeGraphControl graph;
    private readonly TextBlock statusText;
    private readonly TextBlock loadingText;
    private readonly Button retryButton;
    private ReferenceIndexSnapshot? displayedSnapshot;
    private bool closed;

    public ReferenceTreeWindow(ReferenceIndexService referenceIndex, string nodeId)
    {
        this.referenceIndex = referenceIndex;
        this.nodeId = nodeId;
        Title = LocaleService.Get("REFERENCE_TREE_TITLE").Replace("{name}", nodeId, StringComparison.Ordinal);
        Width = 980;
        Height = 620;
        MinWidth = 640;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#202124"));
        FontFamily = Ludork.Services.EditorTheme.FontFamily;
        EditorWindowIcon.Apply(this);
        EditorLayoutService.AttachWindow(this, nameof(ReferenceTreeWindow));
        graph = new ReferenceTreeGraphControl(nodeId);
        graph.NodeOpenRequested += onNodeOpenRequested;
        statusText = new TextBlock
        {
            Foreground = Brushes.LightGray,
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        retryButton = new Button
        {
            Content = LocaleService.Get("RETRY"),
            IsVisible = false,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(12, 0, 0, 0),
        };
        retryButton.Click += (_, _) => referenceIndex.RequestRefresh();
        Grid status = new()
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            Margin = new Thickness(12, 0),
        };
        status.Children.Add(statusText);
        Grid.SetColumn(retryButton, 1);
        status.Children.Add(retryButton);
        loadingText = new TextBlock
        {
            Text = LocaleService.Get("LOADING"),
            Foreground = Brushes.LightGray,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            IsHitTestVisible = false,
        };
        Grid content = new()
        {
            RowDefinitions = new RowDefinitions("40,*"),
        };
        content.Children.Add(status);
        Grid.SetRow(graph, 1);
        content.Children.Add(graph);
        Grid.SetRow(loadingText, 1);
        content.Children.Add(loadingText);
        Content = content;
        KeyDown += onKeyDown;
        Opened += onOpened;
        Closed += onClosed;
    }

    private void onOpened(object? sender, EventArgs args)
    {
        referenceIndex.SnapshotChanged += onSnapshotChanged;
        referenceIndex.RequestRefresh();
        updateSnapshot();
    }

    private void onClosed(object? sender, EventArgs args)
    {
        closed = true;
        referenceIndex.SnapshotChanged -= onSnapshotChanged;
        graph.NodeOpenRequested -= onNodeOpenRequested;
    }

    private void onSnapshotChanged(object? sender, EventArgs args)
    {
        if (Dispatcher.UIThread.CheckAccess())
            updateSnapshot();
        else
            Dispatcher.UIThread.Post(updateSnapshot);
    }

    private void updateSnapshot()
    {
        if (closed)
            return;
        ReferenceIndexSnapshot? snapshot = referenceIndex.CurrentSnapshot;
        if (snapshot is not null && !ReferenceEquals(snapshot, displayedSnapshot))
        {
            displayedSnapshot = snapshot;
            graph.SetSnapshot(snapshot);
            ReferenceNode? node = snapshot.GetNode(nodeId);
            string nodeName = node is null ? nodeId : $"{ReferenceTypeDisplay.Name(node.Type)}: {node.Key}";
            Title = LocaleService.Get("REFERENCE_TREE_TITLE").Replace("{name}", nodeName, StringComparison.Ordinal);
        }
        string? error = referenceIndex.UpdateError;
        bool failed = !string.IsNullOrEmpty(error) && !referenceIndex.IsUpdating;
        statusText.Text = failed
            ? LocaleService.Get("REFERENCE_TREE_UPDATE_FAILED").Replace("{reason}", error, StringComparison.Ordinal)
            : referenceIndex.IsUpdating && displayedSnapshot is not null
                ? LocaleService.Get("REFERENCE_TREE_UPDATING")
                : string.Empty;
        ToolTip.SetTip(statusText, failed ? statusText.Text : null);
        retryButton.IsVisible = failed;
        loadingText.IsVisible = displayedSnapshot is null && !failed;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        Close();
        args.Handled = true;
    }

    private void onNodeOpenRequested(object? sender, ReferenceNodeOpenEventArgs args)
    {
        string path = displayedSnapshot?.GetNodePath(args.NodeId) ?? string.Empty;
        if (!File.Exists(path))
            return;
        try
        {
            Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
        }
        catch (Win32Exception)
        {
        }
        catch (InvalidOperationException)
        {
        }
    }

}
