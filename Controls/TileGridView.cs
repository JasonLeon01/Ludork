using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Ludork.ViewModels;
using System;
using System.ComponentModel;
using System.IO;

namespace Ludork.Controls;

public sealed class TileGridView : Control
{
    private static readonly IBrush BackgroundBrush = new SolidColorBrush(Color.FromRgb(30, 30, 30));
    private static readonly Pen GridPen = new(new SolidColorBrush(Color.FromArgb(150, 80, 80, 80)), 1);
    private static readonly Pen SelectionPen = new(new SolidColorBrush(Color.FromRgb(255, 215, 64)), 2);

    private TileSelectViewModel? viewModel;
    private Bitmap? bitmap;
    private (int X, int Y)? dragStartCell;
    private (int X, int Y)? lastDragCell;

    public TileGridView()
    {
        DataContextChanged += (_, _) => attachViewModel(DataContext as TileSelectViewModel);
    }

    public override void Render(DrawingContext context)
    {
        context.FillRectangle(BackgroundBrush, Bounds);
        if (bitmap is null || viewModel is null)
            return;
        int width = bitmap.PixelSize.Width;
        int height = bitmap.PixelSize.Height;
        Rect imageRect = new Rect(0, 0, width, height);
        context.DrawImage(bitmap, imageRect, imageRect);
        int cell = Math.Max(1, viewModel.CellSize);
        for (int x = 0; x <= width; x += cell)
            context.DrawLine(GridPen, new Point(x, 0), new Point(x, height));
        for (int y = 0; y <= height; y += cell)
            context.DrawLine(GridPen, new Point(0, y), new Point(width, y));
        if (viewModel.SelectedTiles is not { } selection)
            return;
        int columns = Math.Max(1, width / cell);
        int tileX = selection.OriginTileNumber % columns;
        int tileY = selection.OriginTileNumber / columns;
        Rect selectionRect = new Rect(
            tileX * cell + 1,
            tileY * cell + 1,
            selection.Width * cell - 2,
            selection.Height * cell - 2
        );
        context.DrawRectangle(null, SelectionPen, selectionRect);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        return bitmap is null
            ? new Size(0, 0)
            : new Size(bitmap.PixelSize.Width, bitmap.PixelSize.Height);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        if (viewModel is null || !viewModel.IsLayerSelected)
            return;
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || getCell(point.Position) is not { } cell)
            return;
        dragStartCell = cell;
        lastDragCell = cell;
        args.Pointer.Capture(this);
        selectCells(cell, cell);
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (viewModel is null || dragStartCell is null)
            return;
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || getCell(point.Position) is not { } cell)
            return;
        if (cell == lastDragCell)
            return;
        lastDragCell = cell;
        setCells(dragStartCell.Value, cell);
        args.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        dragStartCell = null;
        lastDragCell = null;
        args.Pointer.Capture(null);
    }

    private void attachViewModel(TileSelectViewModel? next)
    {
        if (viewModel is not null)
            viewModel.PropertyChanged -= onViewModelPropertyChanged;
        viewModel = next;
        if (viewModel is not null)
            viewModel.PropertyChanged += onViewModelPropertyChanged;
        loadBitmap();
    }

    private void onViewModelPropertyChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (args.PropertyName == nameof(TileSelectViewModel.SelectedTileset))
            loadBitmap();
        else if (args.PropertyName is nameof(TileSelectViewModel.SelectedTiles) or nameof(TileSelectViewModel.IsLayerSelected))
            InvalidateVisual();
    }

    private void loadBitmap()
    {
        bitmap?.Dispose();
        bitmap = null;
        string? path = viewModel?.SelectedTileset?.ImagePath;
        if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
        {
            bitmap = new Bitmap(path);
        }
        InvalidateMeasure();
        InvalidateVisual();
    }

    private (int X, int Y)? getCell(Point point)
    {
        if (bitmap is null || viewModel is null)
            return null;
        int cell = Math.Max(1, viewModel.CellSize);
        if (point.X < 0 || point.Y < 0 || point.X >= bitmap.PixelSize.Width || point.Y >= bitmap.PixelSize.Height)
            return null;
        return ((int)point.X / cell, (int)point.Y / cell);
    }

    private void selectCells((int X, int Y) first, (int X, int Y) second)
    {
        if (bitmap is null || viewModel is null)
            return;
        int cell = Math.Max(1, viewModel.CellSize);
        int columns = Math.Max(1, bitmap.PixelSize.Width / cell);
        int minX = Math.Min(first.X, second.X);
        int minY = Math.Min(first.Y, second.Y);
        int width = Math.Abs(first.X - second.X) + 1;
        int height = Math.Abs(first.Y - second.Y) + 1;
        viewModel.selectTiles(minY * columns + minX, width, height);
    }

    private void setCells((int X, int Y) first, (int X, int Y) second)
    {
        if (bitmap is null || viewModel is null)
            return;
        int cell = Math.Max(1, viewModel.CellSize);
        int columns = Math.Max(1, bitmap.PixelSize.Width / cell);
        int minX = Math.Min(first.X, second.X);
        int minY = Math.Min(first.Y, second.Y);
        int width = Math.Abs(first.X - second.X) + 1;
        int height = Math.Abs(first.Y - second.Y) + 1;
        viewModel.setTiles(minY * columns + minX, width, height);
    }
}
