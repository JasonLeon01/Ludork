using Avalonia;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Text.Json.Nodes;

namespace Ludork.Views.Utils;

internal sealed class MoveRouteMapReferenceView : MapReferenceView
{
    private readonly List<RouteStep> routeSteps = [];
    private readonly List<(int X, int Y)> routeCells = [];
    private (int X, int Y)? currentCell;
    private bool dragging;

    public MoveRouteMapReferenceView(GameDataService gameData) : base(gameData)
    {
    }

    public event EventHandler? RouteChanged;

    public override void SetMap(string? mapKey, JsonObject? mapData)
    {
        base.SetMap(mapKey, mapData);
        ClearRoute();
    }

    public void SetRoute(JsonNode? value)
    {
        dragging = false;
        currentCell = null;
        routeCells.Clear();
        routeSteps.Clear();
        routeSteps.AddRange(BlueprintNodeParameterValues.GetRouteSteps(value));
        InvalidateVisual();
    }

    public JsonArray GetRoute()
    {
        return BlueprintNodeParameterValues.RouteToJson(routeSteps);
    }

    public void ClearRoute()
    {
        dragging = false;
        currentCell = null;
        routeCells.Clear();
        routeSteps.Clear();
        RouteChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed || GetCell(point.Position) is not { } cell)
            return;
        Focus();
        dragging = true;
        currentCell = cell;
        routeCells.Clear();
        routeCells.Add(cell);
        routeSteps.Clear();
        RouteChanged?.Invoke(this, EventArgs.Empty);
        args.Pointer.Capture(this);
        args.Handled = true;
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        if (!dragging || !args.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        if (GetCell(args.GetPosition(this)) is { } cell)
            appendPathTo(cell);
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (!dragging)
            return;
        if (GetCell(args.GetPosition(this)) is { } cell)
            appendPathTo(cell);
        dragging = false;
        args.Pointer.Capture(null);
        args.Handled = true;
        InvalidateVisual();
    }

    protected override void DrawOverlay(DrawingContext context)
    {
        if (routeCells.Count == 0)
            return;
        Pen routePen = new(
            new SolidColorBrush(Color.FromRgb(32, 180, 255)),
            Math.Max(2, TileSize / 8.0));
        for (int index = 1; index < routeCells.Count; index++)
        {
            (int X, int Y) previous = routeCells[index - 1];
            (int X, int Y) current = routeCells[index];
            context.DrawLine(
                routePen,
                GetCellCenter(previous.X, previous.Y),
                GetCellCenter(current.X, current.Y));
        }
        Point start = GetCellCenter(routeCells[0].X, routeCells[0].Y);
        double radius = Math.Max(4, TileSize / 5.0);
        context.DrawEllipse(new SolidColorBrush(Color.FromArgb(220, 255, 215, 0)), null, start, radius, radius);
        if (routeCells.Count > 1)
        {
            (int X, int Y) lastCell = routeCells[^1];
            Point end = GetCellCenter(lastCell.X, lastCell.Y);
            context.DrawEllipse(new SolidColorBrush(Color.FromArgb(230, 32, 180, 255)), null, end, radius, radius);
        }
    }

    private void appendPathTo((int X, int Y) target)
    {
        if (currentCell is not { } current || current == target)
            return;
        int x = current.X;
        int y = current.Y;
        bool changed = false;
        while (x != target.X)
        {
            int step = target.X > x ? 1 : -1;
            x += step;
            if (!IsInMap(x, y))
                break;
            routeSteps.Add(new RouteStep(step, 0));
            routeCells.Add((x, y));
            changed = true;
        }
        while (y != target.Y)
        {
            int step = target.Y > y ? 1 : -1;
            y += step;
            if (!IsInMap(x, y))
                break;
            routeSteps.Add(new RouteStep(0, step));
            routeCells.Add((x, y));
            changed = true;
        }
        if (!changed)
            return;
        currentCell = (x, y);
        RouteChanged?.Invoke(this, EventArgs.Empty);
        InvalidateVisual();
    }
}
