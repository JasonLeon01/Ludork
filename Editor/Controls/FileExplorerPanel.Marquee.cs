using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Presenters;
using Avalonia.Controls.Selection;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Controls;

public partial class FileExplorerPanel
{
    private readonly HashSet<int> marqueeInitialSelection = [];
    private readonly DispatcherTimer marqueeScrollTimer = new() { Interval = TimeSpan.FromMilliseconds(20) };
    private ListBox? marqueeList;
    private Panel? marqueePanel;
    private ScrollViewer? marqueeScroll;
    private ScrollContentPresenter? marqueeViewport;
    private IPointer? marqueePointer;
    private Point marqueeStart;
    private Point marqueePosition;
    private int marqueeInitialAnchor;
    private int marqueeInitialPrimaryIndex;
    private bool marqueeActive;
    private bool marqueeAdditive;
    private bool marqueeToggle;
    private bool marqueeAutoScrollToSelection;
    private bool updatingMarquee;

    private void initializeMarquee()
    {
        DataContextChanged += (_, _) => endMarquee(false);
        IconEntries.Items.CollectionChanged += (_, _) => endMarquee(false);
        ListEntries.Items.CollectionChanged += (_, _) => endMarquee(false);
        IconEntries.AddHandler(PointerWheelChangedEvent, onMarqueeWheel, RoutingStrategies.Tunnel);
        ListEntries.AddHandler(PointerWheelChangedEvent, onMarqueeWheel, RoutingStrategies.Tunnel);
        AddHandler(KeyDownEvent, onMarqueeKeyDown, RoutingStrategies.Tunnel);
        marqueeScrollTimer.Tick += onMarqueeScrollTick;
    }

    private bool tryBeginMarquee(object? sender, PointerPressedEventArgs args)
    {
        if (sender is not ListBox list || DataContext is not FileExplorerViewModel { IsLoading: false }
            || args.Pointer.Type != PointerType.Mouse || isScrollBarSource(args.Source)
            || getEntry(args.Source) is not null || list.ItemsPanelRoot is not Panel panel)
            return false;
        PointerPointProperties properties = args.GetCurrentPoint(list).Properties;
        if (!properties.IsLeftButtonPressed || properties.IsRightButtonPressed || properties.IsMiddleButtonPressed)
            return false;
        ScrollContentPresenter? viewport = list.GetVisualDescendants().OfType<ScrollContentPresenter>().FirstOrDefault();
        ScrollViewer? scroll = list.GetVisualDescendants().OfType<ScrollViewer>().FirstOrDefault();
        if (viewport is null || scroll is null || !new Rect(viewport.Bounds.Size).Contains(args.GetPosition(viewport)))
            return false;
        list.Focus();
        marqueeList = list;
        marqueePanel = panel;
        marqueeScroll = scroll;
        marqueeViewport = viewport;
        marqueePointer = args.Pointer;
        marqueeStart = args.GetPosition(panel);
        marqueePosition = args.GetPosition(EntriesSelectionSurface);
        marqueeAdditive = args.KeyModifiers.HasFlag(KeyModifiers.Shift);
        marqueeToggle = !marqueeAdditive && EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        marqueeInitialSelection.UnionWith(list.Selection.SelectedIndexes);
        marqueeInitialAnchor = list.Selection.AnchorIndex;
        marqueeInitialPrimaryIndex = list.SelectedIndex;
        marqueeAutoScrollToSelection = list.AutoScrollToSelectedItem;
        list.AutoScrollToSelectedItem = false;
        scroll.ScrollChanged += onMarqueeScrollChanged;
        args.Pointer.Capture(list);
        args.Handled = true;
        return true;
    }

    private bool moveMarquee(PointerEventArgs args)
    {
        if (marqueeList is null || marqueePanel is null || args.Pointer != marqueePointer)
            return false;
        PointerPointProperties properties = args.GetCurrentPoint(marqueeList).Properties;
        if (!properties.IsLeftButtonPressed || properties.IsRightButtonPressed || properties.IsMiddleButtonPressed)
        {
            endMarquee(false);
            return true;
        }
        marqueePosition = args.GetPosition(EntriesSelectionSurface);
        Point current = args.GetPosition(marqueePanel);
        if (!marqueeActive && Math.Abs(current.X - marqueeStart.X) < 4 && Math.Abs(current.Y - marqueeStart.Y) < 4)
            return true;
        marqueeActive = true;
        marqueeScrollTimer.Start();
        updateMarquee();
        args.Handled = true;
        return true;
    }

    private bool releaseMarquee(PointerReleasedEventArgs args)
    {
        if (marqueeList is null || args.Pointer != marqueePointer)
            return false;
        marqueePosition = args.GetPosition(EntriesSelectionSurface);
        if (marqueeActive)
            updateMarquee();
        endMarquee(args.InitialPressMouseButton == MouseButton.Left);
        args.Handled = true;
        return true;
    }

    private Rect marqueeViewportBounds()
    {
        Point origin = marqueeViewport?.TranslatePoint(default, EntriesSelectionSurface) ?? default;
        return new Rect(origin, marqueeViewport?.Bounds.Size ?? default);
    }

    private void updateMarquee()
    {
        if (marqueeList is null || marqueePanel is null || updatingMarquee)
            return;
        updatingMarquee = true;
        try
        {
            marqueeList.UpdateLayout();
            Rect viewport = marqueeViewportBounds();
            Point position = new(Math.Clamp(marqueePosition.X, viewport.Left, viewport.Right),
                Math.Clamp(marqueePosition.Y, viewport.Top, viewport.Bottom));
            if (EntriesSelectionSurface.TranslatePoint(position, marqueePanel) is not Point finish)
                return;
            Rect rectangle = MarqueeSelection.CreateRect(marqueeStart, finish, new Rect(marqueePanel.Bounds.Size));
            Point origin = marqueePanel.TranslatePoint(rectangle.TopLeft, EntriesSelectionSurface) ?? default;
            Rect visible = new Rect(origin, rectangle.Size).Intersect(viewport);
            Canvas.SetLeft(EntriesSelectionBox, visible.X);
            Canvas.SetTop(EntriesSelectionBox, visible.Y);
            EntriesSelectionBox.Width = visible.Width;
            EntriesSelectionBox.Height = visible.Height;
            EntriesSelectionBox.IsVisible = marqueeActive;
            HashSet<int> selection = MarqueeSelection.Build(marqueeInitialSelection, marqueeHitIndexes(rectangle),
                marqueeAdditive, marqueeToggle);
            applyMarqueeSelection(selection, marqueeAdditive || marqueeToggle ? marqueeInitialAnchor : -1);
        }
        finally
        {
            updatingMarquee = false;
        }
    }

    private IEnumerable<int> marqueeHitIndexes(Rect rectangle)
    {
        if (marqueeList is null || marqueePanel is null)
            yield break;
        Control? sample = marqueeList.GetRealizedContainers().FirstOrDefault(container => container.IsVisible && container.Bounds.Height > 0);
        if (sample is null || sample.TranslatePoint(default, marqueePanel) is not Point sampleOrigin)
            yield break;
        int sampleIndex = marqueeList.IndexFromContainer(sample);
        double rowHeight = sample.Bounds.Height + sample.Margin.Top + sample.Margin.Bottom;
        for (int index = 0; index < marqueeList.Items.Count; index++)
        {
            Rect bounds = marqueePanel is VirtualizingTilePanel tiles
                ? tiles.GetItemBounds(index).Deflate(sample.Margin)
                : new Rect(sampleOrigin.X, sampleOrigin.Y + (index - sampleIndex) * rowHeight,
                    sample.Bounds.Width, sample.Bounds.Height);
            if (MarqueeSelection.Overlaps(rectangle, bounds))
                yield return index;
        }
    }

    private void applyMarqueeSelection(HashSet<int> selection, int anchor)
    {
        if (marqueeList is null)
            return;
        using (marqueeList.Selection.BatchUpdate())
        {
            foreach (int index in marqueeList.Selection.SelectedIndexes.ToArray())
                if (!selection.Contains(index))
                    marqueeList.Selection.Deselect(index);
            foreach (int index in selection.Order())
                if (!marqueeList.Selection.IsSelected(index))
                    marqueeList.Selection.Select(index);
            marqueeList.Selection.AnchorIndex = anchor >= 0 ? anchor : selection.DefaultIfEmpty(-1).Min();
        }
    }

    private void onMarqueeScrollChanged(object? sender, ScrollChangedEventArgs args)
    {
        if (marqueeActive)
            updateMarquee();
    }

    private void onMarqueeWheel(object? sender, PointerWheelEventArgs args)
    {
        if (marqueeList is null || marqueeViewport is null || ReferenceEquals(args.Source, marqueeViewport)
            || TopLevel.GetTopLevel(this) is not TopLevel root)
            return;
        PointerWheelEventArgs forwarded = new(marqueeViewport, args.Pointer, root, args.GetPosition(root),
            args.Timestamp, args.GetCurrentPoint(marqueeViewport).Properties, args.KeyModifiers, args.Delta);
        marqueeViewport.RaiseEvent(forwarded);
        args.Handled = true;
    }

    private void onMarqueeScrollTick(object? sender, EventArgs args)
    {
        if (!marqueeActive || marqueeScroll is null)
            return;
        Rect viewport = marqueeViewportBounds();
        double delta = marqueePosition.Y < viewport.Top + 24
            ? -Math.Min(20, viewport.Top + 24 - marqueePosition.Y)
            : marqueePosition.Y > viewport.Bottom - 24
                ? Math.Min(20, marqueePosition.Y - viewport.Bottom + 24)
                : 0;
        if (delta == 0)
            return;
        marqueeScroll.Offset = new Vector(marqueeScroll.Offset.X,
            Math.Clamp(marqueeScroll.Offset.Y + delta, 0, Math.Max(0, marqueeScroll.Extent.Height - marqueeScroll.Viewport.Height)));
        updateMarquee();
    }

    private void onMarqueeKeyDown(object? sender, KeyEventArgs args)
    {
        if (marqueeList is null || args.Key != Key.Escape)
            return;
        using (marqueeList.Selection.BatchUpdate())
        {
            marqueeList.Selection.Clear();
            if (marqueeInitialPrimaryIndex >= 0)
                marqueeList.Selection.Select(marqueeInitialPrimaryIndex);
            foreach (int index in marqueeInitialSelection)
                marqueeList.Selection.Select(index);
            marqueeList.Selection.AnchorIndex = marqueeInitialAnchor;
        }
        endMarquee(false);
        args.Handled = true;
    }

    private void endMarquee(bool clearOnBlank)
    {
        if (marqueeList is null)
            return;
        if (clearOnBlank && !marqueeActive && !marqueeAdditive && !marqueeToggle)
            applyMarqueeSelection([], -1);
        marqueeList.AutoScrollToSelectedItem = marqueeAutoScrollToSelection;
        if (marqueeScroll is not null)
            marqueeScroll.ScrollChanged -= onMarqueeScrollChanged;
        IPointer? pointer = marqueePointer;
        ListBox list = marqueeList;
        marqueeList = null;
        marqueePanel = null;
        marqueeScroll = null;
        marqueeViewport = null;
        marqueePointer = null;
        marqueeActive = false;
        marqueeInitialSelection.Clear();
        marqueeScrollTimer.Stop();
        EntriesSelectionBox.IsVisible = false;
        if (pointer?.Captured == list)
            pointer.Capture(null);
    }
}
