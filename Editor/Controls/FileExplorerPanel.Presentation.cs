using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using Ludork.Models;
using Ludork.ViewModels;
using System;
using System.ComponentModel;
using System.Linq;

namespace Ludork.Controls;

public partial class FileExplorerPanel
{
    private FileExplorerViewModel? zoomViewModel;
    private ZoomAnchor? zoomAnchor;
    private bool zoomRestorePending;

    private void initializeZoom()
    {
        DataContextChanged += (_, _) => attachZoomViewModel();
        AttachedToVisualTree += (_, _) => attachZoomViewModel();
        DetachedFromVisualTree += (_, _) => attachZoomViewModel(null);
        IconEntries.AddHandler(PointerWheelChangedEvent, onZoomWheel, RoutingStrategies.Tunnel);
        ListEntries.AddHandler(PointerWheelChangedEvent, onZoomWheel, RoutingStrategies.Tunnel);
    }

    private void attachZoomViewModel() => attachZoomViewModel(VisualRoot is null ? null : DataContext as FileExplorerViewModel);

    private void attachZoomViewModel(FileExplorerViewModel? next)
    {
        if (ReferenceEquals(zoomViewModel, next))
            return;
        if (zoomViewModel is not null)
        {
            zoomViewModel.PropertyChanging -= onPresentationChanging;
            zoomViewModel.PropertyChanged -= onPresentationChanged;
        }
        zoomAnchor = null;
        zoomViewModel = next;
        if (next is not null)
        {
            next.PropertyChanging += onPresentationChanging;
            next.PropertyChanged += onPresentationChanged;
        }
    }

    private void onZoomWheel(object? sender, PointerWheelEventArgs args)
    {
        if (DataContext is not FileExplorerViewModel viewModel
            || !EditorZoomInput.ShouldZoomWheel(args.KeyModifiers, true)
            || args.Delta.Y == 0)
            return;
        viewModel.Zoom += Math.Sign(args.Delta.Y) * 10;
        args.Handled = true;
    }

    private void onPresentationChanging(object? sender, PropertyChangingEventArgs args)
    {
        if (args.PropertyName != nameof(FileExplorerViewModel.Zoom) || zoomRestorePending)
            return;
        if (sender is FileExplorerViewModel viewModel)
            zoomAnchor = new ZoomAnchor(activeEntries, viewModel, viewModel.PresentationRevision,
                capturePresentationState(activeEntries));
    }

    private void onPresentationChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName != nameof(FileExplorerViewModel.Zoom) || zoomRestorePending)
            return;
        zoomRestorePending = true;
        Dispatcher.UIThread.Post(() =>
        {
            ZoomAnchor? anchor = zoomAnchor;
            if (anchor is null || !canRestorePresentation(anchor.ViewModel, anchor.Revision))
            {
                zoomRestorePending = false;
                zoomAnchor = null;
                return;
            }
            activeEntries.UpdateLayout();
            int index = findEntryIndex(activeEntries, anchor.State.ScrollAnchorPath);
            if (ReferenceEquals(anchor.List, activeEntries) && index >= 0)
            {
                activeEntries.ScrollIntoView(activeEntries.Items[index]!);
                activeEntries.UpdateLayout();
            }
            Dispatcher.UIThread.Post(() => restoreZoomAnchor(anchor), DispatcherPriority.Loaded);
        }, DispatcherPriority.Loaded);
    }

    private void restoreZoomAnchor(ZoomAnchor? anchor)
    {
        if (anchor is not null && ReferenceEquals(zoomAnchor, anchor)
            && ReferenceEquals(anchor.List, activeEntries) && canRestorePresentation(anchor.ViewModel, anchor.Revision))
        {
            restoreScrollPosition(activeEntries, anchor.State);
        }
        zoomRestorePending = false;
        zoomAnchor = null;
        updatePreviewActivity();
    }

    private sealed record ZoomAnchor(ListBox List, FileExplorerViewModel ViewModel, long Revision,
        FileExplorerPresentationState State);
}
