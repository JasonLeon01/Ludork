using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.ViewModels;
using System;
using System.Linq;

namespace Ludork.Views;

public partial class TileSelectPanel : UserControl
{
    private bool autoTilePressed;
    private AutoTileItemViewModel? autoTileBeforeKey;
    private bool autoTileKeyPressed;

    public event EventHandler? SelectionCommitted;

    public TileSelectPanel()
    {
        InitializeComponent();
        TileGrid.SelectionCommitted += (_, _) => SelectionCommitted?.Invoke(this, EventArgs.Empty);
        AutoTileList.AddHandler(PointerPressedEvent, onAutoTilePressed, RoutingStrategies.Bubble, true);
        AutoTileList.AddHandler(PointerReleasedEvent, onAutoTileReleased, RoutingStrategies.Bubble, true);
        AutoTileList.PointerCaptureLost += (_, _) => autoTilePressed = false;
        AutoTileList.LostFocus += (_, _) => { autoTileKeyPressed = false; autoTileBeforeKey = null; };
        AutoTileList.AddHandler(KeyDownEvent, onAutoTileKeyDown, RoutingStrategies.Tunnel);
        AutoTileList.AddHandler(KeyUpEvent, onAutoTileKeyUp, RoutingStrategies.Bubble, true);
    }

    internal Rect? GetSelectedTileBounds(Visual relativeTo)
    {
        if (!IsEffectivelyVisible || !TileGrid.IsEffectivelyVisible
            || TileGrid.GetSelectionBounds() is not { } selection
            || TileGrid.TransformToVisual(relativeTo) is not { } transform)
            return null;
        Rect visible = selection.Intersect(new Rect(TileGrid.Bounds.Size)).TransformToAABB(transform);
        foreach (Visual ancestor in TileGrid.GetVisualAncestors())
        {
            if (!ancestor.ClipToBounds)
                continue;
            if (ancestor.TransformToVisual(relativeTo) is not { } ancestorTransform)
                return null;
            visible = visible.Intersect(new Rect(ancestor.Bounds.Size).TransformToAABB(ancestorTransform));
        }
        visible = visible.Intersect(new Rect(relativeTo.Bounds.Size));
        return visible.Width > 0 && visible.Height > 0 ? visible : null;
    }

    private void onAutoTilePressed(object? sender, PointerPressedEventArgs args)
    {
        autoTilePressed = args.GetCurrentPoint(AutoTileList).Properties.IsLeftButtonPressed
            && args.Source is Avalonia.Visual source
            && (source is ListBoxItem || source.GetVisualAncestors().Any(ancestor => ancestor is ListBoxItem));
        if (autoTilePressed)
            args.Pointer.Capture(AutoTileList);
    }

    private void onAutoTileReleased(object? sender, PointerReleasedEventArgs args)
    {
        bool committed = autoTilePressed && args.InitialPressMouseButton == MouseButton.Left;
        autoTilePressed = false;
        if (args.Pointer.Captured == AutoTileList)
            args.Pointer.Capture(null);
        if (committed && DataContext is TileSelectViewModel { SelectedAutoTile: not null })
            SelectionCommitted?.Invoke(this, EventArgs.Empty);
    }

    private void onAutoTileKeyDown(object? sender, KeyEventArgs args)
    {
        if (autoTileKeyPressed || args.Key is not (Key.Up or Key.Down or Key.Left or Key.Right
            or Key.Home or Key.End or Key.PageUp or Key.PageDown or Key.Space or Key.Enter))
            return;
        autoTileKeyPressed = true;
        autoTileBeforeKey = (DataContext as TileSelectViewModel)?.SelectedAutoTile;
    }

    private void onAutoTileKeyUp(object? sender, KeyEventArgs args)
    {
        if (!autoTileKeyPressed || args.Key is not (Key.Up or Key.Down or Key.Left or Key.Right
            or Key.Home or Key.End or Key.PageUp or Key.PageDown or Key.Space or Key.Enter))
            return;
        autoTileKeyPressed = false;
        if (DataContext is TileSelectViewModel { SelectedAutoTile: { } selected }
            && selected != autoTileBeforeKey)
            SelectionCommitted?.Invoke(this, EventArgs.Empty);
        autoTileBeforeKey = null;
    }
}
