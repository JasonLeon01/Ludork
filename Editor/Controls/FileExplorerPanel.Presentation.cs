using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
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
        ListBox list = activeEntries;
        ScrollViewer? scroll = list.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
        if (scroll is null)
            return;
        foreach (ListBoxItem container in list.GetVisualDescendants().OfType<ListBoxItem>()
            .OrderBy(item => item.TranslatePoint(default, scroll)?.Y ?? double.MaxValue)
            .ThenBy(item => item.TranslatePoint(default, scroll)?.X ?? double.MaxValue))
        {
            if (container.DataContext is not FileExplorerEntryViewModel entry
                || container.TranslatePoint(default, scroll) is not Point origin
                || container.Bounds.Height <= 0
                || origin.Y + container.Bounds.Height <= 0
                || origin.Y >= scroll.Viewport.Height)
                continue;
            zoomAnchor = new ZoomAnchor(list, entry, -origin.Y / container.Bounds.Height);
            break;
        }
    }

    private void onPresentationChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName != nameof(FileExplorerViewModel.Zoom) || zoomRestorePending)
            return;
        zoomRestorePending = true;
        Dispatcher.UIThread.Post(() =>
        {
            ZoomAnchor? anchor = zoomAnchor;
            if (VisualRoot is null)
            {
                zoomRestorePending = false;
                zoomAnchor = null;
                return;
            }
            activeEntries.UpdateLayout();
            if (anchor is not null && ReferenceEquals(anchor.List, activeEntries)
                && activeEntries.Items.Contains(anchor.Entry))
            {
                activeEntries.ScrollIntoView(anchor.Entry);
                activeEntries.UpdateLayout();
            }
            Dispatcher.UIThread.Post(() => restoreZoomAnchor(anchor), DispatcherPriority.Loaded);
        }, DispatcherPriority.Loaded);
    }

    private void restoreZoomAnchor(ZoomAnchor? anchor)
    {
        if (VisualRoot is not null && anchor is not null && ReferenceEquals(zoomAnchor, anchor)
            && ReferenceEquals(anchor.List, activeEntries) && activeEntries.Items.Contains(anchor.Entry))
        {
            activeEntries.UpdateLayout();
            ScrollViewer? scroll = activeEntries.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
            Control? container = activeEntries.ContainerFromItem(anchor.Entry);
            if (scroll is not null && container?.TranslatePoint(default, scroll) is Point origin)
            {
                double offset = scroll.Offset.Y + origin.Y + anchor.Fraction * container.Bounds.Height;
                scroll.Offset = new Vector(scroll.Offset.X, Math.Clamp(offset, 0,
                    Math.Max(0, scroll.Extent.Height - scroll.Viewport.Height)));
            }
        }
        zoomRestorePending = false;
        zoomAnchor = null;
        updatePreviewActivity();
    }

    private sealed record ZoomAnchor(ListBox List, FileExplorerEntryViewModel Entry, double Fraction);
}
