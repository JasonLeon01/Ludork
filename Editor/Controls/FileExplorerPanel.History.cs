using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Selection;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.ViewModels;
using System;
using System.ComponentModel;
using System.Linq;

namespace Ludork.Controls;

public partial class FileExplorerPanel
{
    private FileExplorerViewModel? historyViewModel;
    private FileExplorerEntryViewModel? pressedSelection;
    private bool restoringPresentation;

    private void initializeHistory()
    {
        DataContextChanged += (_, _) => attachHistoryViewModel();
        AttachedToVisualTree += (_, _) => attachHistoryViewModel();
        DetachedFromVisualTree += (_, _) => attachHistoryViewModel(null);
        AddHandler(KeyDownEvent, onHistoryKeyDown, RoutingStrategies.Tunnel);
        AddHandler(PointerPressedEvent, onHistoryPointerPressed, RoutingStrategies.Tunnel);
        IconEntries.AddHandler(KeyDownEvent, onUserSelectionKeyDown, RoutingStrategies.Tunnel);
        ListEntries.AddHandler(KeyDownEvent, onUserSelectionKeyDown, RoutingStrategies.Tunnel);
    }

    private void attachHistoryViewModel() => attachHistoryViewModel(VisualRoot is null ? null : DataContext as FileExplorerViewModel);

    private void attachHistoryViewModel(FileExplorerViewModel? next)
    {
        if (ReferenceEquals(historyViewModel, next))
            return;
        if (historyViewModel is not null)
        {
            historyViewModel.UpdatePresentationState(capturePresentationState(activeEntries));
            historyViewModel.CapturePresentationRequested -= onCapturePresentation;
            historyViewModel.PresentationRestoreRequested -= onRestorePresentation;
            historyViewModel.PropertyChanged -= onBrowsePropertyChanged;
        }
        historyViewModel = next;
        if (next is not null)
        {
            next.CapturePresentationRequested += onCapturePresentation;
            next.PresentationRestoreRequested += onRestorePresentation;
            next.PropertyChanged += onBrowsePropertyChanged;
            if (next.IsActive && !next.IsLoading)
                onRestorePresentation(next, EventArgs.Empty);
        }
    }

    private void onBrowsePropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName is nameof(FileExplorerViewModel.CurrentPath) or nameof(FileExplorerViewModel.SearchText)
            or nameof(FileExplorerViewModel.IsLoading) or nameof(FileExplorerViewModel.IsActive)
            or nameof(FileExplorerViewModel.Zoom))
            endMarquee(false);
        if (args.PropertyName == nameof(FileExplorerViewModel.IsActive))
        {
            updatePreviewActivity();
            if (historyViewModel is { IsActive: true, IsLoading: false } viewModel)
                onRestorePresentation(viewModel, EventArgs.Empty);
        }
        else if (args.PropertyName == nameof(FileExplorerViewModel.SelectedEntry) && !restoringPresentation
            && sender is FileExplorerViewModel { IsActive: true } viewModel
            && !ReferenceEquals(activeEntries.SelectedItem, viewModel.SelectedEntry))
        {
            restoringPresentation = true;
            try
            {
                activeEntries.SelectedItem = viewModel.SelectedEntry;
            }
            finally
            {
                restoringPresentation = false;
            }
        }
    }

    private void onEntriesSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (!restoringPresentation && ReferenceEquals(sender, activeEntries)
            && DataContext is FileExplorerViewModel { IsActive: true } viewModel)
            viewModel.SelectedEntry = activeEntries.SelectedItem as FileExplorerEntryViewModel;
    }

    private void onCapturePresentation(object? sender, EventArgs args)
    {
        if (!restoringPresentation && sender is FileExplorerViewModel viewModel && ReferenceEquals(viewModel, DataContext))
            viewModel.UpdatePresentationState(capturePresentationState(activeEntries));
    }

    private static FileExplorerPresentationState capturePresentationState(ListBox list)
    {
        ScrollViewer? scroll = list.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
        string? scrollPath = null;
        double fraction = 0;
        if (scroll is not null)
        {
            foreach (ListBoxItem container in list.GetVisualDescendants().OfType<ListBoxItem>()
                .OrderBy(item => item.TranslatePoint(default, scroll)?.Y ?? double.MaxValue)
                .ThenBy(item => item.TranslatePoint(default, scroll)?.X ?? double.MaxValue))
            {
                if (container.DataContext is not FileExplorerEntryViewModel entry
                    || container.TranslatePoint(default, scroll) is not Point origin
                    || container.Bounds.Height <= 0 || origin.Y + container.Bounds.Height <= 0
                    || origin.Y >= scroll.Viewport.Height)
                    continue;
                scrollPath = entry.FullPath;
                fraction = -origin.Y / container.Bounds.Height;
                break;
            }
        }
        int anchorIndex = list.Selection.AnchorIndex;
        return new FileExplorerPresentationState(getSelectedEntries(list).Select(entry => entry.FullPath).ToArray(),
            (list.SelectedItem as FileExplorerEntryViewModel)?.FullPath,
            anchorIndex >= 0 && anchorIndex < list.Items.Count
                ? (list.Items[anchorIndex] as FileExplorerEntryViewModel)?.FullPath : null,
            scrollPath, fraction, scroll?.Offset.X ?? 0, scroll?.Offset.Y ?? 0);
    }

    private void onRestorePresentation(object? sender, EventArgs args)
    {
        if (sender is not FileExplorerViewModel viewModel)
            return;
        long revision = viewModel.PresentationRevision;
        FileExplorerPresentationState state = viewModel.PresentationState;
        Dispatcher.UIThread.Post(() =>
        {
            if (!canRestorePresentation(viewModel, revision))
                return;
            restoringPresentation = true;
            try
            {
                ListBox list = activeEntries;
                using (list.Selection.BatchUpdate())
                {
                    list.Selection.Clear();
                    selectPath(list, state.PrimaryPath);
                    foreach (string path in state.SelectedPaths)
                        selectPath(list, path);
                    list.Selection.AnchorIndex = findEntryIndex(list, state.SelectionAnchorPath);
                }
                viewModel.SelectedEntry = list.SelectedItem as FileExplorerEntryViewModel;
                list.UpdateLayout();
                int scrollIndex = findEntryIndex(list, state.ScrollAnchorPath);
                if (scrollIndex >= 0)
                {
                    list.ScrollIntoView(list.Items[scrollIndex]!);
                    list.UpdateLayout();
                }
                Dispatcher.UIThread.Post(() =>
                {
                    if (!canRestorePresentation(viewModel, revision))
                        return;
                    restoreScrollPosition(activeEntries, state);
                    viewModel.CompletePresentationRestore(revision);
                    updatePreviewActivity();
                }, DispatcherPriority.Loaded);
            }
            finally
            {
                restoringPresentation = false;
            }
        }, DispatcherPriority.Loaded);
    }

    private bool canRestorePresentation(FileExplorerViewModel viewModel, long revision) => VisualRoot is not null
        && ReferenceEquals(DataContext, viewModel) && viewModel.IsActive && !viewModel.IsLoading
        && viewModel.PresentationRevision == revision;

    private static void selectPath(ListBox list, string? path)
    {
        int index = findEntryIndex(list, path);
        if (index >= 0)
            list.Selection.Select(index);
    }

    private static int findEntryIndex(ListBox list, string? path)
    {
        if (path is null)
            return -1;
        StringComparison comparison = OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        for (int index = 0; index < list.Items.Count; index++)
            if (list.Items[index] is FileExplorerEntryViewModel entry && string.Equals(entry.FullPath, path, comparison))
                return index;
        return -1;
    }

    private static void restoreScrollPosition(ListBox list, FileExplorerPresentationState state)
    {
        list.UpdateLayout();
        ScrollViewer? scroll = list.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
        if (scroll is null)
            return;
        double offset = state.VerticalOffset;
        int index = findEntryIndex(list, state.ScrollAnchorPath);
        Control? container = index >= 0 ? list.ContainerFromIndex(index) : null;
        if (container?.TranslatePoint(default, scroll) is Point origin)
            offset = scroll.Offset.Y + origin.Y + state.ScrollAnchorFraction * container.Bounds.Height;
        scroll.Offset = new Vector(Math.Clamp(state.HorizontalOffset, 0, Math.Max(0, scroll.Extent.Width - scroll.Viewport.Width)),
            Math.Clamp(offset, 0, Math.Max(0, scroll.Extent.Height - scroll.Viewport.Height)));
    }

    private async void onBack(object? sender, RoutedEventArgs args)
    {
        if (DataContext is FileExplorerViewModel viewModel)
            await viewModel.GoBackAsync();
    }

    private async void onForward(object? sender, RoutedEventArgs args)
    {
        if (DataContext is FileExplorerViewModel viewModel)
            await viewModel.GoForwardAsync();
    }

    private async void onHistoryKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.KeyModifiers != KeyModifiers.Alt || args.Key is not (Key.Left or Key.Right)
            || DataContext is not FileExplorerViewModel viewModel)
            return;
        args.Handled = true;
        if (args.Key == Key.Left)
            await viewModel.GoBackAsync();
        else
            await viewModel.GoForwardAsync();
    }

    private async void onHistoryPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        PointerUpdateKind kind = args.GetCurrentPoint(this).Properties.PointerUpdateKind;
        if (kind is not (PointerUpdateKind.XButton1Pressed or PointerUpdateKind.XButton2Pressed)
            || DataContext is not FileExplorerViewModel viewModel)
            return;
        args.Handled = true;
        if (kind == PointerUpdateKind.XButton1Pressed)
            await viewModel.GoBackAsync();
        else
            await viewModel.GoForwardAsync();
    }

    private void onUserSelectionKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key is not (Key.Up or Key.Down or Key.Home or Key.End or Key.PageUp or Key.PageDown)
            || args.KeyModifiers.HasFlag(KeyModifiers.Alt) || DataContext is not FileExplorerViewModel viewModel)
            return;
        FileExplorerEntryViewModel? previous = viewModel.SelectedEntry;
        long revision = viewModel.PresentationRevision;
        Dispatcher.UIThread.Post(async () =>
        {
            if (ReferenceEquals(DataContext, viewModel) && revision == viewModel.PresentationRevision
                && viewModel.SelectedEntry is FileExplorerEntryViewModel entry && !ReferenceEquals(previous, entry))
                await viewModel.NotifyUserSelectionAsync(entry);
        }, DispatcherPriority.Input);
    }
}
