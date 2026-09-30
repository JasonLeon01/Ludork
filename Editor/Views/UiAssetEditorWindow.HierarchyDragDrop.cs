using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Presenters;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Services.UiAssets;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views;

public partial class UiAssetEditorWindow
{
    private const string DragPrefix = "ludork-ui-node-name:";
    private const string PaletteDragPrefix = "ludork-ui-control-id:";
    private const string DropInsideClass = "drop-inside";
    private const string DropBeforeClass = "drop-before";
    private const string DropAfterClass = "drop-after";
    private PointerPressedEventArgs? hierarchyDragPress;
    private Point? hierarchyDragStart;
    private string? hierarchyDragText;
    private DragDropEffects hierarchyDragEffects;
    private TreeViewItem? hierarchyDropIndicatorItem;
    private string? hierarchyDropIndicatorClass;
    private bool startingHierarchyDrag;

    private void configureHierarchyDragDrop()
    {
        HierarchyTree.AddHandler(
            PointerPressedEvent,
            onHierarchyPointerPressed,
            RoutingStrategies.Tunnel);
        HierarchyTree.AddHandler(
            InputElement.ContextRequestedEvent,
            onHierarchyContextRequested,
            RoutingStrategies.Tunnel);
        HierarchyTree.PointerMoved += onHierarchyPointerMoved;
        HierarchyTree.PointerReleased += onHierarchyPointerReleased;
        HierarchyTree.PointerCaptureLost += onHierarchyPointerCaptureLost;
        DragDrop.SetAllowDrop(HierarchyTree, true);
        HierarchyTree.AddHandler(DragDrop.DragOverEvent, onHierarchyDragOver);
        HierarchyTree.AddHandler(DragDrop.DragLeaveEvent, onHierarchyDragLeave);
        HierarchyTree.AddHandler(DragDrop.DropEvent, onHierarchyDrop);
    }

    private void onHierarchyPointerPressed(
        object? sender,
        PointerPressedEventArgs args)
    {
        clearHierarchyDrag();
        PointerPoint point = args.GetCurrentPoint(this);
        UiHierarchyItem? item = getHierarchyItem(args.Source);
        if (!point.Properties.IsLeftButtonPressed || point.Properties.IsRightButtonPressed
            || item is null
            || document.FindParent(item.NodeName) is null)
        {
            return;
        }
        hierarchyDragPress = args;
        hierarchyDragStart = point.Position;
        hierarchyDragText = DragPrefix + item.NodeName;
        hierarchyDragEffects = DragDropEffects.Move;
    }

    private void onPalettePointerPressed(object? sender, PointerPressedEventArgs args)
    {
        clearHierarchyDrag();
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || point.Properties.IsRightButtonPressed || args.ClickCount != 1
            || sender is not Button { Tag: UiControlDescriptor descriptor })
        {
            return;
        }
        hierarchyDragPress = args;
        hierarchyDragStart = point.Position;
        hierarchyDragText = PaletteDragPrefix + descriptor.ControlId;
        hierarchyDragEffects = DragDropEffects.Copy;
    }

    private void onHierarchyContextRequested(
        object? sender,
        ContextRequestedEventArgs args)
    {
        clearHierarchyDrag();
        if (!args.TryGetPosition(HierarchyTree, out _))
            return;
        UiHierarchyItem? item = getHierarchyItem(args.Source);
        if (item is null)
            return;
        showHierarchyContextMenu(item);
        args.Handled = true;
    }

    private async void onHierarchyPointerMoved(
        object? sender,
        PointerEventArgs args)
    {
        if (startingHierarchyDrag
            || hierarchyDragStart is not Point start
            || hierarchyDragPress is null
            || hierarchyDragText is null)
        {
            return;
        }
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || point.Properties.IsRightButtonPressed)
        {
            clearHierarchyDrag();
            return;
        }
        Point current = point.Position;
        if (Math.Abs(current.X - start.X) < 4
            && Math.Abs(current.Y - start.Y) < 4)
        {
            return;
        }
        startingHierarchyDrag = true;
        DataTransfer data = new();
        data.Add(DataTransferItem.CreateText(hierarchyDragText));
        await DragDrop.DoDragDropAsync(
            hierarchyDragPress,
            data,
            hierarchyDragEffects);
        startingHierarchyDrag = false;
        clearHierarchyDropIndicator();
        clearHierarchyDrag();
    }

    private void onHierarchyPointerReleased(
        object? sender,
        PointerReleasedEventArgs args)
    {
        clearHierarchyDrag();
    }

    private void onHierarchyPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        clearHierarchyDrag();
        clearHierarchyDropIndicator();
    }

    private void onHierarchyDragOver(object? sender, DragEventArgs args)
    {
        string? nodeName = getDraggedValue(args, DragPrefix);
        string? controlId = getDraggedValue(args, PaletteDragPrefix);
        UiHierarchyItem? target = getHierarchyDropTarget(args.Source);
        TreeViewItem? container = getHierarchyDropContainer(args.Source, target);
        args.DragEffects = DragDropEffects.None;
        if (target is null)
        {
            clearHierarchyDropIndicator();
            args.Handled = true;
            return;
        }
        UiAssetEditingService.DropPosition position = resolveHierarchyDropPosition(
            args, target, nodeName, controlId);
        string? parentName = null;
        if (nodeName is not null
            && document.TryGetDropLocation(nodeName, target.NodeName, position, out parentName, out _))
        {
            args.DragEffects = DragDropEffects.Move;
        }
        else if (controlId is not null
            && document.TryGetControlDropLocation(
                controlId, target.NodeName, position, out parentName, out _, out _))
        {
            args.DragEffects = DragDropEffects.Copy;
        }
        setHierarchyDropIndicator(
            container,
            getHierarchyDropIndicatorClass(
                target.NodeName,
                parentName,
                position,
                args.DragEffects != DragDropEffects.None));
        args.Handled = true;
    }

    private void onHierarchyDragLeave(object? sender, RoutedEventArgs args)
    {
        if (args is DragEventArgs drag)
        {
            Point position = drag.GetPosition(HierarchyTree);
            if (position.X >= 0
                && position.Y >= 0
                && position.X <= HierarchyTree.Bounds.Width
                && position.Y <= HierarchyTree.Bounds.Height)
            {
                return;
            }
        }
        clearHierarchyDropIndicator();
    }

    private async void onHierarchyDrop(object? sender, DragEventArgs args)
    {
        clearHierarchyDropIndicator();
        string? nodeName = getDraggedValue(args, DragPrefix);
        string? controlId = getDraggedValue(args, PaletteDragPrefix);
        UiHierarchyItem? target = getHierarchyDropTarget(args.Source);
        if (target is null)
            return;
        args.Handled = true;
        args.DragEffects = DragDropEffects.None;
        flushPendingField();
        UiAssetEditingService.DropPosition position = resolveHierarchyDropPosition(
            args, target, nodeName, controlId);
        if (controlId is not null
            && controlLookup.TryGetValue(controlId, out UiControlDescriptor? descriptor)
            && document.TryGetControlDropLocation(controlId, target.NodeName, position,
                out string addParent, out int addIndex, out _))
        {
            addControl(descriptor, addParent, addIndex);
            args.DragEffects = DragDropEffects.Copy;
        }
        else if (nodeName is not null
            && document.TryGetDropLocation(nodeName, target.NodeName, position,
                out string parentName, out int index))
        {
            args.DragEffects = DragDropEffects.Move;
            await moveNodeAsync(nodeName, parentName, index);
        }
    }

    private UiAssetEditingService.DropPosition resolveHierarchyDropPosition(
        DragEventArgs args,
        UiHierarchyItem target,
        string? nodeName,
        string? controlId)
    {
        UiAssetEditingService.DropPosition position = getDropPosition(args, out double relativeY);
        if (position != UiAssetEditingService.DropPosition.Inside
            || isHierarchyInsideChildDrop(target.NodeName, nodeName, controlId))
        {
            return position;
        }
        return relativeY < 0.5
            ? UiAssetEditingService.DropPosition.Before
            : UiAssetEditingService.DropPosition.After;
    }

    private bool isHierarchyInsideChildDrop(
        string targetNodeName,
        string? nodeName,
        string? controlId)
    {
        if (nodeName is not null
            && document.TryGetDropLocation(
                nodeName,
                targetNodeName,
                UiAssetEditingService.DropPosition.Inside,
                out string parentName,
                out _)
            && string.Equals(parentName, targetNodeName, StringComparison.Ordinal))
        {
            return true;
        }
        return controlId is not null
            && document.TryGetControlDropLocation(
                controlId,
                targetNodeName,
                UiAssetEditingService.DropPosition.Inside,
                out string controlParentName,
                out _,
                out _)
            && string.Equals(controlParentName, targetNodeName, StringComparison.Ordinal);
    }

    private static UiAssetEditingService.DropPosition getDropPosition(
        DragEventArgs args,
        out double relativeY)
    {
        TreeViewItem? container = getHierarchyContainer(args.Source);
        Control? header = container?.GetVisualDescendants().OfType<ContentPresenter>()
            .FirstOrDefault(presenter => presenter.Name == "PART_HeaderPresenter"
                && ReferenceEquals(presenter.TemplatedParent, container));
        relativeY = header is null
            ? 0.5
            : args.GetPosition(header).Y / Math.Max(1, header.Bounds.Height);
        return relativeY < 0.25
            ? UiAssetEditingService.DropPosition.Before
            : relativeY > 0.75
                ? UiAssetEditingService.DropPosition.After
                : UiAssetEditingService.DropPosition.Inside;
    }

    private static string? getHierarchyDropIndicatorClass(
        string targetNodeName,
        string? parentName,
        UiAssetEditingService.DropPosition position,
        bool valid)
    {
        if (!valid || string.IsNullOrEmpty(parentName))
            return null;
        if (string.Equals(parentName, targetNodeName, StringComparison.Ordinal))
            return DropInsideClass;
        return position == UiAssetEditingService.DropPosition.After
            ? DropAfterClass
            : DropBeforeClass;
    }

    private void setHierarchyDropIndicator(TreeViewItem? container, string? className)
    {
        if (ReferenceEquals(hierarchyDropIndicatorItem, container)
            && string.Equals(hierarchyDropIndicatorClass, className, StringComparison.Ordinal))
        {
            return;
        }
        clearHierarchyDropIndicator();
        if (container is null || className is null)
            return;
        container.Classes.Add(className);
        hierarchyDropIndicatorItem = container;
        hierarchyDropIndicatorClass = className;
    }

    private void clearHierarchyDropIndicator()
    {
        if (hierarchyDropIndicatorItem is not null && hierarchyDropIndicatorClass is not null)
            hierarchyDropIndicatorItem.Classes.Remove(hierarchyDropIndicatorClass);
        hierarchyDropIndicatorItem = null;
        hierarchyDropIndicatorClass = null;
    }

    private UiHierarchyItem? getHierarchyDropTarget(object? source)
    {
        return getHierarchyItem(source)
            ?? (HierarchyTree.ItemsSource as IEnumerable<UiHierarchyItem>)?.FirstOrDefault();
    }

    private static UiHierarchyItem? getHierarchyItem(object? source)
    {
        if (source is Control { DataContext: UiHierarchyItem item })
            return item;
        return (source as Visual)?
            .GetVisualAncestors()
            .OfType<Control>()
            .Select(control => control.DataContext)
            .OfType<UiHierarchyItem>()
            .FirstOrDefault();
    }

    private static TreeViewItem? getHierarchyContainer(object? source)
    {
        if (source is TreeViewItem item)
            return item;
        return (source as Visual)?
            .GetVisualAncestors()
            .OfType<TreeViewItem>()
            .FirstOrDefault();
    }

    private TreeViewItem? getHierarchyDropContainer(object? source, UiHierarchyItem? target)
    {
        TreeViewItem? container = getHierarchyContainer(source);
        if (container is not null)
            return container;
        if (target is null)
            return null;
        return HierarchyTree.GetVisualDescendants()
            .OfType<TreeViewItem>()
            .FirstOrDefault(item => item.DataContext is UiHierarchyItem current
                && string.Equals(current.NodeName, target.NodeName, StringComparison.Ordinal));
    }

    private static string? getDraggedValue(DragEventArgs args, string prefix)
    {
        string? text = args.DataTransfer.TryGetText();
        return text is not null && text.StartsWith(prefix, StringComparison.Ordinal)
            ? text[prefix.Length..]
            : null;
    }

    private void clearHierarchyDrag()
    {
        hierarchyDragPress = null;
        hierarchyDragStart = null;
        hierarchyDragText = null;
    }
}
