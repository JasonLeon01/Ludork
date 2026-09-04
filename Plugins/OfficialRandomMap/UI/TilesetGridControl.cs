using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.VisualTree;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using System;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class TilesetGridControl : Control, IDisposable
{
    private const double MinimumScale = 0.5;
    private const double MaximumScale = 4.0;
    private static readonly IBrush BackgroundBrush =
        new SolidColorBrush(Color.Parse("#1e1e1e"));
    private static readonly Pen GridPen =
        new(new SolidColorBrush(Color.FromArgb(145, 80, 80, 80)), 1);
    private static readonly Pen SelectionPen =
        new(new SolidColorBrush(Color.Parse("#fbbc04")), 3);

    private readonly PluginTilesetSnapshot tileset;
    private readonly Bitmap bitmap;
    private readonly EditorZoomInput zoomInput = new();
    private ScrollViewer? hostScrollViewer;
    private double scale = 1.0;
    private TilesetZoomAnchor? pendingZoomAnchor;

    public TilesetGridControl(
        IMapEditorHost host,
        PluginTilesetSnapshot tileset,
        int? selectedTile)
    {
        this.tileset = tileset;
        string filePath = host.ResolveAssetFile(tileset.AssetPath);
        if (filePath.Length == 0)
            throw new InvalidOperationException("Tileset asset is unavailable.");
        bitmap = new Bitmap(filePath);
        SelectedTile = selectedTile;
        Focusable = true;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public int? SelectedTile { get; private set; }

    public event EventHandler? SelectionChanged;

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(BackgroundBrush, Bounds);
        Rect destination = new(
            0,
            0,
            bitmap.PixelSize.Width * scale,
            bitmap.PixelSize.Height * scale);
        Rect source = new(
            0,
            0,
            bitmap.PixelSize.Width,
            bitmap.PixelSize.Height);
        context.DrawImage(bitmap, source, destination);
        double cellWidth = tileset.TileWidth * scale;
        double cellHeight = tileset.TileHeight * scale;
        for (double x = 0; x <= destination.Width; x += cellWidth)
            context.DrawLine(GridPen, new Point(x, 0), new Point(x, destination.Height));
        for (double y = 0; y <= destination.Height; y += cellHeight)
            context.DrawLine(GridPen, new Point(0, y), new Point(destination.Width, y));
        if (SelectedTile is not int selected)
            return;
        int columns = Math.Max(
            1,
            bitmap.PixelSize.Width / Math.Max(1, tileset.TileWidth));
        int xIndex = selected % columns;
        int yIndex = selected / columns;
        Rect selection = new(
            xIndex * cellWidth + 1.5,
            yIndex * cellHeight + 1.5,
            cellWidth - 3,
            cellHeight - 3);
        context.DrawRectangle(null, SelectionPen, selection);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        return new Size(
            bitmap.PixelSize.Width * scale,
            bitmap.PixelSize.Height * scale);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed)
            return;
        int x = (int)(point.Position.X / scale) / Math.Max(1, tileset.TileWidth);
        int y = (int)(point.Position.Y / scale) / Math.Max(1, tileset.TileHeight);
        int columns = Math.Max(
            1,
            bitmap.PixelSize.Width / Math.Max(1, tileset.TileWidth));
        int rows = Math.Max(
            1,
            bitmap.PixelSize.Height / Math.Max(1, tileset.TileHeight));
        if (x < 0 || y < 0 || x >= columns || y >= rows)
            return;
        int tile = y * columns + x;
        if (tile < 0 || tile >= tileset.TileCount)
            return;
        SelectedTile = tile;
        SelectionChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
        args.Handled = true;
    }

    protected override void OnAttachedToVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        LayoutUpdated += onLayoutUpdated;
        hostScrollViewer = this.FindAncestorOfType<ScrollViewer>();
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingZoomAnchor = null;
        hostScrollViewer = null;
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
        if (!EditorZoomInput.ShouldZoomWheel(args.KeyModifiers, false))
            return;
        double delta = args.Delta.Y > 0 ? 0.25 : args.Delta.Y < 0 ? -0.25 : 0;
        if (delta == 0)
            return;
        setScale(
            scale + delta,
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
        double nextScale = EditorZoomInput.ScaleByFactor(
            scale,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            MinimumScale,
            MaximumScale);
        setScale(
            nextScale,
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

    private void setScale(
        double nextScale,
        Point contentPoint,
        Point viewportPoint)
    {
        double clampedScale = Math.Clamp(
            nextScale,
            MinimumScale,
            MaximumScale);
        if (Math.Abs(clampedScale - scale) < double.Epsilon)
            return;
        pendingZoomAnchor = new TilesetZoomAnchor(
            contentPoint.X / scale,
            contentPoint.Y / scale,
            viewportPoint);
        scale = clampedScale;
        InvalidateMeasure();
        InvalidateVisual();
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (pendingZoomAnchor is null)
            return;
        applyZoomAnchor();
    }

    private void applyZoomAnchor()
    {
        if (pendingZoomAnchor is not TilesetZoomAnchor anchor
            || hostScrollViewer is null)
        {
            pendingZoomAnchor = null;
            return;
        }
        pendingZoomAnchor = null;
        Point contentAnchor = new(
            anchor.ImageX * scale,
            anchor.ImageY * scale);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

    public void Dispose()
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingZoomAnchor = null;
        hostScrollViewer = null;
        bitmap.Dispose();
    }

    private readonly record struct TilesetZoomAnchor(
        double ImageX,
        double ImageY,
        Point ViewportPoint);
}
