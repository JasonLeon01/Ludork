using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Ludork.Models;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;

namespace Ludork.Views;

public sealed class ReferenceTreeWindow : Window
{
    private readonly ReferenceIndexService referenceIndex;

    public ReferenceTreeWindow(ReferenceIndexService referenceIndex, string nodeId)
    {
        this.referenceIndex = referenceIndex;
        ReferenceNode? node = referenceIndex.GetNode(nodeId);
        string nodeName = node is null ? nodeId : $"{getTypeName(node.Type)}: {node.Key}";
        Title = LocaleService.Get("REFERENCE_TREE_TITLE").Replace("{name}", nodeName, StringComparison.Ordinal);
        Width = 980;
        Height = 620;
        MinWidth = 640;
        MinHeight = 420;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#202124"));
        FontFamily = FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
        EditorWindowIcon.Apply(this);
        ReferenceTreeGraphControl graph = new(referenceIndex, nodeId);
        graph.NodeOpenRequested += onNodeOpenRequested;
        Content = graph;
        KeyDown += onKeyDown;
    }

    private void onKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key != Key.Escape)
            return;
        Close();
        args.Handled = true;
    }

    private void onNodeOpenRequested(object? sender, ReferenceNodeOpenEventArgs args)
    {
        string path = referenceIndex.GetNodePath(args.NodeId);
        if (!File.Exists(path))
            return;
        try
        {
            Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
        }
        catch (Win32Exception)
        {
        }
        catch (InvalidOperationException)
        {
        }
    }

    private static string getTypeName(string type)
    {
        string key = type switch
        {
            "asset" => "REFERENCE_TYPE_ASSET",
            "autoTile" => "REFERENCE_TYPE_AUTOTILE",
            "blueprint" => "REFERENCE_TYPE_BLUEPRINT",
            "commonFunction" => "REFERENCE_TYPE_COMMON_FUNCTION",
            "config" => "REFERENCE_TYPE_CONFIG",
            "general" => "REFERENCE_TYPE_GENERAL",
            "generalMember" => "REFERENCE_TYPE_GENERAL_MEMBER",
            "map" => "REFERENCE_TYPE_MAP",
            "animation" => "REFERENCE_TYPE_ANIMATION",
            "tileset" => "REFERENCE_TYPE_TILESET",
            _ => "REFERENCE_TYPE_UNKNOWN",
        };
        return LocaleService.Get(key);
    }
}

public sealed class ReferenceNodeOpenEventArgs(string nodeId) : EventArgs
{
    public string NodeId { get; } = nodeId;
}

internal sealed class ReferenceTreeGraphControl : Control
{
    private const int MaximumDepth = 5;
    private const double HorizontalStep = 360;
    private const double VerticalStep = 125;
    private const double NodeWidth = 240;
    private const double NodeHeight = 74;
    private static readonly FontFamily GraphFont =
        FontFamily.Parse("avares://Ludork/Assets/HarmonyOS_Sans_SC_Regular.ttf#HarmonyOS Sans SC");
    private static readonly Typeface NormalTypeface = new(GraphFont);
    private static readonly Typeface BoldTypeface = new(GraphFont, FontStyle.Normal, FontWeight.Bold);
    private static readonly IReadOnlyDictionary<string, Color> NodeColors =
        new Dictionary<string, Color>(StringComparer.Ordinal)
        {
            ["asset"] = Color.FromRgb(70, 98, 118),
            ["autoTile"] = Color.FromRgb(102, 120, 70),
            ["blueprint"] = Color.FromRgb(86, 92, 142),
            ["commonFunction"] = Color.FromRgb(120, 88, 138),
            ["config"] = Color.FromRgb(118, 105, 72),
            ["general"] = Color.FromRgb(112, 88, 72),
            ["generalMember"] = Color.FromRgb(124, 96, 80),
            ["map"] = Color.FromRgb(72, 118, 96),
            ["animation"] = Color.FromRgb(128, 84, 102),
            ["tileset"] = Color.FromRgb(82, 118, 118),
            ["unknown"] = Color.FromRgb(82, 82, 82),
        };

    private readonly ReferenceIndexService referenceIndex;
    private readonly string rootNodeId;
    private readonly List<ReferenceGraphNode> nodes = [];
    private readonly List<ReferenceGraphEdge> edges = [];
    private readonly EditorZoomInput zoomInput = new();
    private int visualSerial;
    private double graphWidth;
    private double graphHeight;
    private double scale = 1;
    private Vector translation;
    private Point dragStart;
    private Vector dragOrigin;
    private bool dragging;
    private bool fitted;
    private ReferenceGraphNode? hoveredNode;

    public ReferenceTreeGraphControl(ReferenceIndexService referenceIndex, string rootNodeId)
    {
        this.referenceIndex = referenceIndex;
        this.rootNodeId = rootNodeId;
        Focusable = true;
        ClipToBounds = true;
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
        buildGraph();
        SizeChanged += (_, _) =>
        {
            if (!fitted && Bounds.Width > 0 && Bounds.Height > 0)
            {
                fitGraph();
                fitted = true;
            }
        };
    }

    public event EventHandler<ReferenceNodeOpenEventArgs>? NodeOpenRequested;

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#202124")), new Rect(Bounds.Size));
        Matrix transform = new(scale, 0, 0, scale, translation.X, translation.Y);
        using (context.PushTransform(transform))
        {
            drawEdges(context);
            foreach (ReferenceGraphNode node in nodes)
                drawNode(context, node);
        }
    }

    protected override void OnPointerPressed(PointerPressedEventArgs args)
    {
        base.OnPointerPressed(args);
        PointerPoint point = args.GetCurrentPoint(this);
        if (!point.Properties.IsLeftButtonPressed && !point.Properties.IsMiddleButtonPressed)
            return;
        Focus();
        ReferenceGraphNode? hit = hitTest(args.GetPosition(this));
        if (args.ClickCount == 2 && hit is not null && hit.NodeId.Length != 0)
        {
            NodeOpenRequested?.Invoke(this, new ReferenceNodeOpenEventArgs(hit.NodeId));
            args.Handled = true;
            return;
        }
        dragging = true;
        dragStart = args.GetPosition(this);
        dragOrigin = translation;
        args.Pointer.Capture(this);
        args.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs args)
    {
        base.OnPointerMoved(args);
        Point position = args.GetPosition(this);
        if (dragging)
        {
            Vector delta = position - dragStart;
            translation = clampTranslation(dragOrigin + delta);
            InvalidateVisual();
            args.Handled = true;
            return;
        }
        ReferenceGraphNode? next = hitTest(position);
        if (ReferenceEquals(next, hoveredNode))
            return;
        hoveredNode = next;
        Cursor = next is not null && next.NodeId.Length != 0
            ? new Cursor(StandardCursorType.Hand)
            : Cursor.Default;
        ToolTip.SetTip(this, next?.Tooltip);
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs args)
    {
        base.OnPointerReleased(args);
        if (!dragging)
            return;
        dragging = false;
        args.Pointer.Capture(null);
        args.Handled = true;
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs args)
    {
        base.OnPointerWheelChanged(args);
        if (zoomInput.ShouldSuppressWheel())
        {
            args.Handled = true;
            return;
        }
        if (EditorZoomInput.IsMacOS
            && !EditorZoomInput.HasPrimaryModifier(args.KeyModifiers))
        {
            translation = clampTranslation(
                EditorZoomInput.GetPannedTranslation(translation, args.Delta));
            InvalidateVisual();
            args.Handled = true;
            return;
        }
        if (args.Delta.Y == 0)
            return;
        Point position = args.GetPosition(this);
        double nextScale = Math.Clamp(scale + (args.Delta.Y > 0 ? 0.1 : -0.1), 0.2, 2.0);
        applyScale(position, nextScale);
        args.Handled = true;
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        Point position = args.GetPosition(this);
        double nextScale = EditorZoomInput.ScaleByFactor(
            scale,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            0.2,
            2.0);
        applyScale(position, nextScale);
        args.Handled = true;
    }

    private void applyScale(Point position, double nextScale)
    {
        if (Math.Abs(nextScale - scale) < double.Epsilon)
            return;
        Point graphPoint = screenToGraph(position);
        scale = nextScale;
        translation = new Vector(
            position.X - graphPoint.X * scale,
            position.Y - graphPoint.Y * scale);
        translation = clampTranslation(translation);
        InvalidateVisual();
    }

    private void buildGraph()
    {
        string rootVisual = createNode(rootNodeId, 0, 0, null, false);
        ReferenceTreeNode leftTree = referenceIndex.GetTree(rootNodeId, ReferenceDirection.ReferencedBy, MaximumDepth);
        ReferenceTreeNode rightTree = referenceIndex.GetTree(rootNodeId, ReferenceDirection.References, MaximumDepth);
        if (leftTree.Items.Count != 0)
        {
            List<string> branchVisuals = [];
            createBranch(leftTree.Items, rootVisual, ReferenceDirection.ReferencedBy, -1, 1, [0], branchVisuals);
            centerNodes(branchVisuals);
        }
        else
        {
            createEmptyNode(LocaleService.Get("REFERENCE_REFERENCED_BY"), -HorizontalStep, -80);
        }
        if (rightTree.Items.Count != 0)
        {
            List<string> branchVisuals = [];
            createBranch(rightTree.Items, rootVisual, ReferenceDirection.References, 1, 1, [0], branchVisuals);
            centerNodes(branchVisuals);
        }
        else
        {
            createEmptyNode(LocaleService.Get("REFERENCE_REFERENCES"), HorizontalStep, -80);
        }
        normalizeGraphPositions();
    }

    private void createBranch(
        IReadOnlyList<ReferenceTreeItem> items,
        string parentVisual,
        ReferenceDirection direction,
        int xSign,
        int depth,
        int[] yCursor,
        ICollection<string> branchVisuals)
    {
        foreach (ReferenceTreeItem item in items)
        {
            ReferenceTreeNode child = item.Child;
            string visual = createNode(
                child.NodeId,
                xSign * depth * HorizontalStep,
                yCursor[0],
                item.Reference,
                child.Cycle);
            branchVisuals.Add(visual);
            yCursor[0] += (int)VerticalStep;
            edges.Add(direction == ReferenceDirection.ReferencedBy
                ? new ReferenceGraphEdge(visual, parentVisual)
                : new ReferenceGraphEdge(parentVisual, visual));
            if (!child.Cycle && child.Items.Count != 0)
                createBranch(child.Items, visual, direction, xSign, depth + 1, yCursor, branchVisuals);
        }
    }

    private string createNode(
        string nodeId,
        double x,
        double y,
        ReferenceRecord? record,
        bool cycle)
    {
        string visualId = $"reference_{visualSerial++}";
        ReferenceNode referenceNode = referenceIndex.GetNode(nodeId)
            ?? new ReferenceNode(nodeId, "unknown", nodeId);
        Color baseColor = NodeColors.TryGetValue(referenceNode.Type, out Color known)
            ? known
            : NodeColors["unknown"];
        bool current = nodeId == rootNodeId;
        Color color = cycle
            ? Color.FromRgb(112, 78, 78)
            : current
                ? lighten(baseColor, 28)
                : baseColor;
        string content = cycle
            ? $"{referenceNode.Key} ({LocaleService.Get("REFERENCE_CYCLE")})"
            : referenceNode.Key;
        nodes.Add(new ReferenceGraphNode(
            visualId,
            nodeId,
            x,
            y,
            getTypeName(referenceNode.Type),
            content,
            color,
            current,
            formatTooltip(referenceNode, record)));
        return visualId;
    }

    private string createEmptyNode(string title, double x, double y)
    {
        string visualId = $"reference_{visualSerial++}";
        nodes.Add(new ReferenceGraphNode(
            visualId,
            string.Empty,
            x,
            y,
            title,
            LocaleService.Get("REFERENCE_NONE"),
            Color.Parse("#3a3a3a"),
            false,
            $"{title}: {LocaleService.Get("REFERENCE_NONE")}"));
        return visualId;
    }

    private void centerNodes(IReadOnlyCollection<string> visualIds)
    {
        ReferenceGraphNode[] branchNodes = nodes
            .Where(node => visualIds.Contains(node.VisualId))
            .ToArray();
        if (branchNodes.Length == 0)
            return;
        double minimum = branchNodes.Min(node => node.Y);
        double maximum = branchNodes.Max(node => node.Y);
        double offset = -((minimum + maximum) / 2.0);
        foreach (ReferenceGraphNode node in branchNodes)
            node.Y += offset;
    }

    private void normalizeGraphPositions()
    {
        const double padding = 80;
        double minimumX = nodes.Min(node => node.X);
        double minimumY = nodes.Min(node => node.Y);
        double maximumX = nodes.Max(node => node.X + NodeWidth);
        double maximumY = nodes.Max(node => node.Y + NodeHeight);
        foreach (ReferenceGraphNode node in nodes)
        {
            node.X = node.X - minimumX + padding;
            node.Y = node.Y - minimumY + padding;
        }
        graphWidth = maximumX - minimumX + padding * 2;
        graphHeight = maximumY - minimumY + padding * 2;
    }

    private void fitGraph()
    {
        double xScale = Math.Max(0.2, (Bounds.Width - 24) / Math.Max(1, graphWidth));
        double yScale = Math.Max(0.2, (Bounds.Height - 24) / Math.Max(1, graphHeight));
        scale = Math.Min(1.0, Math.Min(xScale, yScale));
        translation = new Vector(
            (Bounds.Width - graphWidth * scale) / 2,
            (Bounds.Height - graphHeight * scale) / 2);
        translation = clampTranslation(translation);
        InvalidateVisual();
    }

    private Vector clampTranslation(Vector value)
    {
        double scaledWidth = graphWidth * scale;
        double scaledHeight = graphHeight * scale;
        double x = scaledWidth <= Bounds.Width
            ? (Bounds.Width - scaledWidth) / 2
            : Math.Clamp(value.X, Bounds.Width - scaledWidth - 12, 12);
        double y = scaledHeight <= Bounds.Height
            ? (Bounds.Height - scaledHeight) / 2
            : Math.Clamp(value.Y, Bounds.Height - scaledHeight - 12, 12);
        return new Vector(x, y);
    }

    private Point screenToGraph(Point point)
    {
        return new Point(
            (point.X - translation.X) / Math.Max(0.0001, scale),
            (point.Y - translation.Y) / Math.Max(0.0001, scale));
    }

    private ReferenceGraphNode? hitTest(Point screenPoint)
    {
        Point graphPoint = screenToGraph(screenPoint);
        return nodes.LastOrDefault(node => node.Bounds.Contains(graphPoint));
    }

    private void drawEdges(DrawingContext context)
    {
        Pen pen = new(new SolidColorBrush(Color.Parse("#9aa0a6")), 2);
        foreach (ReferenceGraphEdge edge in edges)
        {
            ReferenceGraphNode? source = nodes.FirstOrDefault(node => node.VisualId == edge.Source);
            ReferenceGraphNode? target = nodes.FirstOrDefault(node => node.VisualId == edge.Target);
            if (source is null || target is null)
                continue;
            bool sourceOnLeft = source.X < target.X;
            double sourceX = sourceOnLeft ? source.X + NodeWidth : source.X;
            double targetX = sourceOnLeft ? target.X : target.X + NodeWidth;
            double sourceY = source.Y + NodeHeight / 2;
            double targetY = target.Y + NodeHeight / 2;
            double middle = (sourceX + targetX) / 2;
            context.DrawLine(pen, new Point(sourceX, sourceY), new Point(middle, sourceY));
            context.DrawLine(pen, new Point(middle, sourceY), new Point(middle, targetY));
            context.DrawLine(pen, new Point(middle, targetY), new Point(targetX, targetY));
        }
    }

    private static void drawNode(DrawingContext context, ReferenceGraphNode node)
    {
        Rect bounds = node.Bounds;
        SolidColorBrush fill = new(node.Color);
        context.FillRectangle(fill, bounds, 6);
        context.DrawRectangle(
            new Pen(new SolidColorBrush(node.Current ? Color.Parse("#f5c65c") : Color.Parse("#b0b3b8")), node.Current ? 3 : 1),
            bounds,
            6);
        Color headerColor = darken(node.Color, 0.84);
        context.FillRectangle(new SolidColorBrush(headerColor), new Rect(node.X, node.Y, NodeWidth, 28), 6);
        context.FillRectangle(new SolidColorBrush(headerColor), new Rect(node.X, node.Y + 22, NodeWidth, 6));
        Color textColor = node.Current ? Color.Parse("#ffebb0") : Colors.White;
        drawText(context, ellipsizeEnd(node.Title, 28), new Point(node.X + 10, node.Y + 6), BoldTypeface, 13, textColor, NodeWidth - 20, 20);
        drawText(context, ellipsizeMiddle(node.Content, 31), new Point(node.X + 10, node.Y + 40), NormalTypeface, 13, textColor, NodeWidth - 20, 22);
        SolidColorBrush portFill = new(Color.Parse("#c5c7ca"));
        Pen portBorder = new(new SolidColorBrush(Color.Parse("#55585c")), 1);
        context.DrawEllipse(portFill, portBorder, new Point(node.X, node.Y + NodeHeight / 2), 5, 5);
        context.DrawEllipse(portFill, portBorder, new Point(node.X + NodeWidth, node.Y + NodeHeight / 2), 5, 5);
    }

    private static void drawText(
        DrawingContext context,
        string text,
        Point origin,
        Typeface typeface,
        double size,
        Color color,
        double width,
        double height)
    {
        using (context.PushClip(new Rect(origin, new Size(width, height))))
        {
            FormattedText formatted = new(
                text,
                CultureInfo.CurrentUICulture,
                FlowDirection.LeftToRight,
                typeface,
                size,
                new SolidColorBrush(color));
            context.DrawText(formatted, origin);
        }
    }

    private string formatTooltip(ReferenceNode node, ReferenceRecord? record)
    {
        List<string> lines = [$"{getTypeName(node.Type)}: {node.Key}"];
        if (record is not null)
        {
            string kind = getKindName(record.Kind);
            lines.Add(record.Path.Length == 0 ? kind : $"{kind} - {record.Path}");
        }
        string path = referenceIndex.GetNodePath(node.Id);
        if (path.Length != 0)
            lines.Add(path);
        return string.Join(Environment.NewLine, lines);
    }

    private static string getTypeName(string type)
    {
        string key = type switch
        {
            "asset" => "REFERENCE_TYPE_ASSET",
            "autoTile" => "REFERENCE_TYPE_AUTOTILE",
            "blueprint" => "REFERENCE_TYPE_BLUEPRINT",
            "commonFunction" => "REFERENCE_TYPE_COMMON_FUNCTION",
            "config" => "REFERENCE_TYPE_CONFIG",
            "general" => "REFERENCE_TYPE_GENERAL",
            "generalMember" => "REFERENCE_TYPE_GENERAL_MEMBER",
            "map" => "REFERENCE_TYPE_MAP",
            "animation" => "REFERENCE_TYPE_ANIMATION",
            "tileset" => "REFERENCE_TYPE_TILESET",
            _ => "REFERENCE_TYPE_UNKNOWN",
        };
        return LocaleService.Get(key);
    }

    private static string getKindName(string kind)
    {
        string key = kind switch
        {
            "animationAsset" => "REFERENCE_KIND_ANIMATION_ASSET",
            "asset" => "REFERENCE_KIND_ASSET",
            "autoTile" => "REFERENCE_KIND_AUTOTILE",
            "blueprintPath" => "REFERENCE_KIND_BLUEPRINT_PATH",
            "configFile" => "REFERENCE_KIND_CONFIG_FILE",
            "mapActor" => "REFERENCE_KIND_MAP_ACTOR",
            "member" => "REFERENCE_KIND_MEMBER",
            "nodeParam" => "REFERENCE_KIND_NODE_PARAM",
            "parent" => "REFERENCE_KIND_PARENT",
            "tileset" => "REFERENCE_KIND_TILESET",
            _ => "REFERENCE_KIND_REFERENCE",
        };
        return LocaleService.Get(key);
    }

    private static Color lighten(Color color, byte amount)
    {
        return Color.FromRgb(
            (byte)Math.Min(255, color.R + amount),
            (byte)Math.Min(255, color.G + amount),
            (byte)Math.Min(255, color.B + amount));
    }

    private static Color darken(Color color, double factor)
    {
        return Color.FromRgb(
            (byte)(color.R * factor),
            (byte)(color.G * factor),
            (byte)(color.B * factor));
    }

    private static string ellipsizeEnd(string text, int maximumCharacters)
    {
        return text.Length <= maximumCharacters
            ? text
            : text[..Math.Max(1, maximumCharacters - 1)] + "…";
    }

    private static string ellipsizeMiddle(string text, int maximumCharacters)
    {
        if (text.Length <= maximumCharacters)
            return text;
        int left = (maximumCharacters - 1) / 2;
        int right = maximumCharacters - 1 - left;
        return text[..left] + "…" + text[^right..];
    }

    private sealed class ReferenceGraphNode(
        string visualId,
        string nodeId,
        double x,
        double y,
        string title,
        string content,
        Color color,
        bool current,
        string tooltip)
    {
        public string VisualId { get; } = visualId;
        public string NodeId { get; } = nodeId;
        public double X { get; set; } = x;
        public double Y { get; set; } = y;
        public string Title { get; } = title;
        public string Content { get; } = content;
        public Color Color { get; } = color;
        public bool Current { get; } = current;
        public string Tooltip { get; } = tooltip;
        public Rect Bounds => new(X, Y, NodeWidth, NodeHeight);
    }

    private sealed record ReferenceGraphEdge(string Source, string Target);
}
