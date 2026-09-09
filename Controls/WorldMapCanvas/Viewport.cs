using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using System;

namespace Ludork.Controls;

public sealed partial class WorldMapCanvas
{
    private const double CanvasPadding = 24;
    private const double MinimumCellSize = 0.02;
    private const double MaximumCellSize = 96;
    private const double InitialMaximumCellSize = 32;
    private const double WheelZoomFactor = 1.16;
    private readonly EditorZoomInput zoomInput = new();
    private ScrollViewer? hostScrollViewer;
    private double cellSize = 8;
    private bool viewInitialized;
    private bool viewportResetPending;
    private bool panning;
    private Point lastPanPointer;
    private WorldMapZoomAnchor? pendingZoomAnchor;

    protected override Size MeasureOverride(Size availableSize)
    {
        Size viewport = getViewportSize(availableSize);
        if (WorldWidth <= 0 || WorldHeight <= 0)
            return viewport;
        return new Size(
            Math.Max(viewport.Width, WorldWidth * cellSize + CanvasPadding * 2),
            Math.Max(viewport.Height, WorldHeight * cellSize + CanvasPadding * 2));
    }

    protected override void OnAttachedToVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        LayoutUpdated += onLayoutUpdated;
        bindHostScrollViewer(this.FindAncestorOfType<ScrollViewer>());
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingZoomAnchor = null;
        bindHostScrollViewer(null);
        base.OnDetachedFromVisualTree(args);
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs args)
    {
        base.OnPointerWheelChanged(args);
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (!EditorZoomInput.ShouldZoomWheel(args.KeyModifiers, true)
            || args.Delta.Y == 0)
        {
            return;
        }
        double factor = Math.Pow(
            WheelZoomFactor,
            Math.Clamp(args.Delta.Y, -8, 8));
        setCellSize(
            EditorZoomInput.ScaleByFactor(
                cellSize,
                factor,
                MinimumCellSize,
                MaximumCellSize),
            args.GetPosition(this),
            getZoomViewportPoint(args));
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        setCellSize(
            EditorZoomInput.ScaleByFactor(
                cellSize,
                EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
                MinimumCellSize,
                MaximumCellSize),
            args.GetPosition(this),
            getZoomViewportPoint(args));
        args.Handled = true;
    }

    private Point getZoomViewportPoint(PointerEventArgs args)
    {
        return hostScrollViewer is null
            ? args.GetPosition(this)
            : args.GetPosition(hostScrollViewer);
    }

    private void setCellSize(
        double nextCellSize,
        Point contentPoint,
        Point viewportPoint)
    {
        if (Math.Abs(nextCellSize - cellSize) < double.Epsilon)
            return;
        Rect worldRect = getWorldRect();
        pendingZoomAnchor = new WorldMapZoomAnchor(
            (contentPoint.X - worldRect.X) / cellSize,
            (contentPoint.Y - worldRect.Y) / cellSize,
            viewportPoint);
        cellSize = nextCellSize;
        viewInitialized = true;
        viewportResetPending = false;
        InvalidateMeasure();
        InvalidateVisual();
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (!viewInitialized)
        {
            tryInitializeView();
            return;
        }
        if (pendingZoomAnchor is not null)
        {
            applyZoomAnchor();
            return;
        }
        if (viewportResetPending && hostScrollViewer is not null)
        {
            viewportResetPending = false;
            hostScrollViewer.Offset = default;
        }
    }

    private void tryInitializeView()
    {
        if (WorldWidth <= 0 || WorldHeight <= 0)
            return;
        Size viewport = getViewportSize(default);
        if (viewport.Width <= 0 || viewport.Height <= 0)
            return;
        double availableWidth = Math.Max(1, viewport.Width - CanvasPadding * 2);
        double availableHeight = Math.Max(1, viewport.Height - CanvasPadding * 2);
        cellSize = Math.Clamp(
            Math.Min(
                availableWidth / WorldWidth,
                availableHeight / WorldHeight),
            MinimumCellSize,
            InitialMaximumCellSize);
        viewInitialized = true;
        InvalidateMeasure();
        InvalidateVisual();
    }

    private void applyZoomAnchor()
    {
        if (pendingZoomAnchor is not WorldMapZoomAnchor anchor
            || hostScrollViewer is null)
        {
            pendingZoomAnchor = null;
            return;
        }
        pendingZoomAnchor = null;
        Rect worldRect = getWorldRect();
        Point contentAnchor = new(
            worldRect.X + anchor.WorldX * cellSize,
            worldRect.Y + anchor.WorldY * cellSize);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

    private bool tryStartPanning(
        PointerPressedEventArgs args,
        PointerPoint point)
    {
        if (!point.Properties.IsMiddleButtonPressed
            || hostScrollViewer is null)
        {
            return false;
        }
        panning = true;
        lastPanPointer = args.GetPosition(hostScrollViewer);
        args.Pointer.Capture(this);
        args.Handled = true;
        return true;
    }

    private bool tryUpdatePanning(PointerEventArgs args)
    {
        if (!panning || hostScrollViewer is null)
            return false;
        Point position = args.GetPosition(hostScrollViewer);
        Vector delta = position - lastPanPointer;
        lastPanPointer = position;
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            new Point(
                hostScrollViewer.Offset.X - delta.X,
                hostScrollViewer.Offset.Y - delta.Y),
            default,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
        args.Handled = true;
        return true;
    }

    private bool tryStopPanning(PointerReleasedEventArgs args)
    {
        if (!panning)
            return false;
        panning = false;
        args.Pointer.Capture(null);
        args.Handled = true;
        return true;
    }

    private void cancelPanning()
    {
        panning = false;
    }

    private Rect getWorldRect()
    {
        double width = WorldWidth * cellSize;
        double height = WorldHeight * cellSize;
        return new Rect(
            Math.Max(0, (Bounds.Width - width) / 2),
            Math.Max(0, (Bounds.Height - height) / 2),
            width,
            height);
    }

    private Point screenToWorld(Point position)
    {
        Rect worldRect = getWorldRect();
        return new Point(
            (position.X - worldRect.X) / cellSize,
            (position.Y - worldRect.Y) / cellSize);
    }

    private Rect worldToScreen(Rect rect)
    {
        Rect worldRect = getWorldRect();
        return new Rect(
            worldRect.X + rect.X * cellSize,
            worldRect.Y + rect.Y * cellSize,
            rect.Width * cellSize,
            rect.Height * cellSize);
    }

    private Rect getVisibleCanvasRect()
    {
        return hostScrollViewer is null
            ? new Rect(0, 0, Bounds.Width, Bounds.Height)
            : new Rect(
                hostScrollViewer.Offset.X,
                hostScrollViewer.Offset.Y,
                hostScrollViewer.Viewport.Width,
                hostScrollViewer.Viewport.Height);
    }

    private Size getViewportSize(Size availableSize)
    {
        double width = hostScrollViewer?.Viewport.Width ?? 0;
        double height = hostScrollViewer?.Viewport.Height ?? 0;
        if (width <= 0 && double.IsFinite(availableSize.Width))
            width = Math.Max(0, availableSize.Width);
        if (height <= 0 && double.IsFinite(availableSize.Height))
            height = Math.Max(0, availableSize.Height);
        return new Size(width, height);
    }

    private void resetViewport()
    {
        cellSize = 8;
        viewInitialized = false;
        viewportResetPending = true;
        pendingZoomAnchor = null;
    }

    private void disposeViewport()
    {
        PointerTouchPadGestureMagnify -= onPointerTouchPadGestureMagnify;
        LayoutUpdated -= onLayoutUpdated;
        pendingZoomAnchor = null;
        bindHostScrollViewer(null);
    }

    private void bindHostScrollViewer(ScrollViewer? scrollViewer)
    {
        if (ReferenceEquals(hostScrollViewer, scrollViewer))
            return;
        if (hostScrollViewer is not null)
            hostScrollViewer.ScrollChanged -= onHostScrollChanged;
        hostScrollViewer = scrollViewer;
        if (hostScrollViewer is not null)
            hostScrollViewer.ScrollChanged += onHostScrollChanged;
    }

    private void onHostScrollChanged(
        object? sender,
        ScrollChangedEventArgs args)
    {
        InvalidateVisual();
    }

    private readonly record struct WorldMapZoomAnchor(
        double WorldX,
        double WorldY,
        Point ViewportPoint);
}
