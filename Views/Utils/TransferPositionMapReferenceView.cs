using Avalonia;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

internal sealed class TransferPositionMapReferenceView : MapReferenceView
{
    private (int X, int Y)? selectedCell;
    private (int X, int Y)? hoverCell;

    public TransferPositionMapReferenceView(GameDataService gameData) : base(gameData)
    {
    }

    public event EventHandler? PositionChanged;

    public JsonArray? GetPosition()
    {
        return selectedCell is { } position ? new JsonArray(position.X, position.Y) : null;
    }

    public void SetPosition(JsonNode? value)
    {
        JsonArray? position = BlueprintNodeParameterValues.NormalizePosition(value);
        selectedCell = position is null
            ? null
            : (position[0]!.GetValue<int>(), position[1]!.GetValue<int>());
        InvalidateVisual();
    }

    public void ClearPosition()
    {
        selectedCell = null;
        PositionChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || GetCell(point.Position) is not { } cell)
            return;
        Focus();
        selectedCell = cell;
        PositionChanged?.Invoke(this, EventArgs.Empty);
        args.Handled = true;
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        (int X, int Y)? cell = GetCell(args.GetPosition(this));
        if (hoverCell == cell)
            return;
        hoverCell = cell;
        InvalidateVisual();
    }

    protected override void OnPointerExited(PointerEventArgs args)
    {
        base.OnPointerExited(args);
        hoverCell = null;
        InvalidateVisual();
    }

    protected override void DrawOverlay(DrawingContext context)
    {
        if (hoverCell is { } hover && hover != selectedCell)
            context.FillRectangle(new SolidColorBrush(Color.FromArgb(35, 255, 255, 255)), GetCellRect(hover.X, hover.Y));
        if (selectedCell is not { } selected || !IsInMap(selected.X, selected.Y))
            return;
        Rect cell = GetCellRect(selected.X, selected.Y);
        context.FillRectangle(new SolidColorBrush(Color.FromArgb(90, 32, 180, 255)), cell);
        Rect outline = new(cell.X + 1, cell.Y + 1, Math.Max(0, cell.Width - 2), Math.Max(0, cell.Height - 2));
        context.DrawRectangle(null, new Pen(new SolidColorBrush(Color.FromRgb(32, 180, 255)), 2), outline);
        Point center = GetCellCenter(selected.X, selected.Y);
        double half = Math.Max(3, TileSize / 4.0);
        Pen crossPen = new(new SolidColorBrush(Color.FromRgb(255, 215, 0)), 2);
        context.DrawLine(crossPen, new Point(center.X - half, center.Y), new Point(center.X + half, center.Y));
        context.DrawLine(crossPen, new Point(center.X, center.Y - half), new Point(center.X, center.Y + half));
    }
}
