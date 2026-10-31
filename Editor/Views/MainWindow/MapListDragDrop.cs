using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Presenters;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views;

public partial class MainWindow
{
    private const string MapListDragPrefix = "ludork-map-list-key:";
    private PointerPressedEventArgs? mapListDragPress;
    private Point? mapListDragStart;
    private MapListItemViewModel? mapListDragSource;
    private bool startingMapListDrag;

    private void configureMapListDragDrop()
    {
        MapList.AddHandler(PointerPressedEvent, onMapListPointerPressed, RoutingStrategies.Tunnel);
        MapList.PointerMoved += onMapListPointerMoved;
        MapList.PointerReleased += onMapListPointerReleased;
        MapList.PointerCaptureLost += onMapListPointerCaptureLost;
        DragDrop.SetAllowDrop(MapList, true);
        MapList.AddHandler(DragDrop.DragOverEvent, onMapListDragOver);
        MapList.AddHandler(DragDrop.DragLeaveEvent, onMapListDragLeave);
        MapList.AddHandler(DragDrop.DropEvent, onMapListDrop);
    }

    private void onMapListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        clearMapListDrag();
        PointerPoint point = args.GetCurrentPoint(this);
        if (viewModel?.CanEdit != true || !point.Properties.IsLeftButtonPressed || point.Properties.IsRightButtonPressed
            || getMapListItem(args.Source)?.DataContext is not MapListItemViewModel item)
            return;
        mapListDragPress = args;
        mapListDragStart = point.Position;
        mapListDragSource = item;
    }

    private async void onMapListPointerMoved(object? sender, PointerEventArgs args)
    {
        if (startingMapListDrag || mapListDragStart is not Point start || mapListDragPress is null
            || mapListDragSource is null)
            return;
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || point.Properties.IsRightButtonPressed)
        {
            clearMapListDrag();
            return;
        }
        Point current = point.Position;
        if (Math.Abs(current.X - start.X) < 4 && Math.Abs(current.Y - start.Y) < 4)
            return;
        startingMapListDrag = true;
        DataTransfer data = new();
        data.Add(DataTransferItem.CreateText(MapListDragPrefix + mapListDragSource.Key));
        await DragDrop.DoDragDropAsync(mapListDragPress, data, DragDropEffects.Move);
        startingMapListDrag = false;
        clearMapListDropIndicator();
        clearMapListDrag();
    }

    private void onMapListPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        if (!startingMapListDrag)
            clearMapListDrag();
    }

    private void onMapListPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        if (!startingMapListDrag)
            clearMapListDrag();
        clearMapListDropIndicator();
    }

    private void onMapListDragOver(object? sender, DragEventArgs args)
    {
        args.DragEffects = DragDropEffects.None;
        args.Handled = true;
        if (viewModel is null || getDraggedMapListSource(args) is not MapListItemViewModel source)
        {
            clearMapListDropIndicator();
            return;
        }
        TreeViewItem? container = getMapListItem(args.Source);
        MapListItemViewModel? target = container?.DataContext as MapListItemViewModel;
        MapOrderDropPosition position = resolveMapListDropPosition(args, source, target);
        if (!viewModel.MapWorkspace.CanDropMap(source, target, position))
        {
            clearMapListDropIndicator();
            return;
        }
        args.DragEffects = DragDropEffects.Move;
        showMapListDropIndicator(target is null ? getLastMapListRootContainer() : container, position);
    }

    private void onMapListDragLeave(object? sender, RoutedEventArgs args)
    {
        if (args is DragEventArgs drag)
        {
            Point position = drag.GetPosition(MapList);
            if (position.X >= 0 && position.Y >= 0
                && position.X <= MapList.Bounds.Width && position.Y <= MapList.Bounds.Height)
                return;
        }
        clearMapListDropIndicator();
    }

    private async void onMapListDrop(object? sender, DragEventArgs args)
    {
        clearMapListDropIndicator();
        if (viewModel is null || getDraggedMapListSource(args) is not MapListItemViewModel source)
            return;
        args.Handled = true;
        args.DragEffects = DragDropEffects.None;
        MapListItemViewModel? target = getMapListItem(args.Source)?.DataContext as MapListItemViewModel;
        MapOrderDropPosition position = resolveMapListDropPosition(args, source, target);
        MapWorkspaceViewModel workspace = viewModel.MapWorkspace;
        if (!workspace.CanDropMap(source, target, position))
            return;
        args.DragEffects = DragDropEffects.Move;
        if (!workspace.IsMapFolderMove(source, target, position))
        {
            workspace.ReorderMap(source, target, position);
            return;
        }
        WorldMapMutationResult? result = await EditorEditWorkflow.RunAsync<WorldMapMutationResult?>(
            this, viewModel.GameData, LocaleService.Get("EDIT_OPERATION_MOVING"), async progress =>
            {
                IReadOnlyList<ReferenceRecord> references =
                    await workspace.MapMoveReferencesAsync(source, target, position, progress);
                if (references.Count != 0)
                {
                    await EditorEditWorkflow.RunInteractionAsync(progress, this, async _ =>
                    {
                        await showMapReferenceBlockAsync(references);
                        return false;
                    });
                    return null;
                }
                return await workspace.MoveMapAsync(source, target, position, progress);
            }, null);
        if (result is { Success: false })
            await AlertDialog.ShowAsync(this, LocaleService.Get("ERROR"), result.Details);
    }

    private MapOrderDropPosition resolveMapListDropPosition(DragEventArgs args, MapListItemViewModel source,
        MapListItemViewModel? target)
    {
        if (target is null || viewModel is null)
            return MapOrderDropPosition.After;
        TreeViewItem? container = getMapListItem(args.Source);
        Control? header = container is null ? null : getMapListHeader(container);
        double relativeY = header is null ? 0.5 : args.GetPosition(header).Y / Math.Max(1, header.Bounds.Height);
        MapOrderDropPosition position = relativeY < 0.25 ? MapOrderDropPosition.Before
            : relativeY > 0.75 ? MapOrderDropPosition.After : MapOrderDropPosition.Inside;
        if (position != MapOrderDropPosition.Inside
            || viewModel.MapWorkspace.CanDropMap(source, target, MapOrderDropPosition.Inside))
            return position;
        return relativeY < 0.5 ? MapOrderDropPosition.Before : MapOrderDropPosition.After;
    }

    private MapListItemViewModel? getDraggedMapListSource(DragEventArgs args)
    {
        string? text = args.DataTransfer.TryGetText();
        return mapListDragSource is not null && text is not null && text.StartsWith(MapListDragPrefix, StringComparison.Ordinal)
            && string.Equals(text[MapListDragPrefix.Length..], mapListDragSource.Key, StringComparison.Ordinal)
                ? mapListDragSource : null;
    }

    private void showMapListDropIndicator(TreeViewItem? container, MapOrderDropPosition position)
    {
        Control? header = container is null ? null : getMapListHeader(container);
        if (container is null || header is null
            || header.TranslatePoint(default, MapListDropOverlay) is not Point headerOrigin
            || container.TranslatePoint(default, MapListDropOverlay) is not Point itemOrigin)
        {
            clearMapListDropIndicator();
            return;
        }
        double width = Math.Max(0, MapListDropOverlay.Bounds.Width - headerOrigin.X);
        if (position == MapOrderDropPosition.Inside)
        {
            Canvas.SetLeft(MapListDropHighlight, headerOrigin.X);
            Canvas.SetTop(MapListDropHighlight, headerOrigin.Y);
            MapListDropHighlight.Width = width;
            MapListDropHighlight.Height = header.Bounds.Height;
            MapListDropHighlight.IsVisible = true;
            MapListDropLine.IsVisible = false;
            return;
        }
        double y = Math.Clamp(
            position == MapOrderDropPosition.Before ? headerOrigin.Y : itemOrigin.Y + container.Bounds.Height,
            1, Math.Max(1, MapListDropOverlay.Bounds.Height - 1));
        MapListDropLine.StartPoint = new Point(headerOrigin.X, y);
        MapListDropLine.EndPoint = new Point(headerOrigin.X + width, y);
        MapListDropLine.IsVisible = true;
        MapListDropHighlight.IsVisible = false;
    }

    private void clearMapListDropIndicator()
    {
        MapListDropHighlight.IsVisible = false;
        MapListDropLine.IsVisible = false;
    }

    private TreeViewItem? getLastMapListRootContainer() =>
        MapList.ItemCount == 0 ? null : MapList.ContainerFromIndex(MapList.ItemCount - 1) as TreeViewItem;

    private static Control? getMapListHeader(TreeViewItem container) =>
        container.GetVisualDescendants().OfType<ContentPresenter>()
            .FirstOrDefault(presenter => presenter.Name == "PART_HeaderPresenter"
                && ReferenceEquals(presenter.TemplatedParent, container));

    private void clearMapListDrag()
    {
        mapListDragPress = null;
        mapListDragStart = null;
        mapListDragSource = null;
    }
}
