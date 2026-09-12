using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Generators;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Threading;
using Avalonia.VisualTree;
using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;

namespace Ludork.Controls;

public sealed class VirtualizingTilePanel : VirtualizingPanel
{
    public static readonly StyledProperty<double> ItemWidthProperty =
        AvaloniaProperty.Register<VirtualizingTilePanel, double>(
            nameof(ItemWidth), 162, validate: value => double.IsFinite(value) && value > 0);

    public static readonly StyledProperty<double> ItemHeightProperty =
        AvaloniaProperty.Register<VirtualizingTilePanel, double>(
            nameof(ItemHeight), 138, validate: value => double.IsFinite(value) && value > 0);

    private readonly Dictionary<int, ContainerEntry> realized = [];
    private readonly Dictionary<object, Stack<Control>> recyclePool = [];
    private readonly HashSet<Control> ownContainers = [];
    private Rect viewport;
    private int columns = 1;
    private int requestedIndex = -1;
    private double viewportHeight;

    static VirtualizingTilePanel()
    {
        AffectsMeasure<VirtualizingTilePanel>(ItemWidthProperty, ItemHeightProperty);
    }

    public VirtualizingTilePanel()
    {
        EffectiveViewportChanged += onEffectiveViewportChanged;
    }

    public double ItemWidth
    {
        get => GetValue(ItemWidthProperty);
        set => SetValue(ItemWidthProperty, value);
    }

    public double ItemHeight
    {
        get => GetValue(ItemHeightProperty);
        set => SetValue(ItemHeightProperty, value);
    }

    protected override Size MeasureOverride(Size availableSize)
    {
        double width = double.IsFinite(availableSize.Width)
            ? availableSize.Width
            : Bounds.Width > 0 ? Bounds.Width : ItemWidth;
        columns = Math.Max(1, (int)Math.Floor(width / ItemWidth));
        viewportHeight = viewport.Height > 0 && double.IsFinite(viewport.Height)
            ? viewport.Height
            : double.IsFinite(availableSize.Height) && availableSize.Height > 0
                ? availableSize.Height
                : this.FindAncestorOfType<ScrollViewer>()?.Viewport.Height is > 0 and var height
                    ? height
                    : ItemHeight * 4;
        double top = Math.Max(0, viewport.Y);
        int firstRow = Math.Max(0, (int)Math.Floor((top - viewportHeight) / ItemHeight));
        int lastRow = (int)Math.Ceiling((top + viewportHeight * 2) / ItemHeight);
        int first = Math.Min(Items.Count, firstRow * columns);
        int last = Math.Min(Items.Count, lastRow * columns);
        foreach (int index in realized.Keys.ToArray())
        {
            if ((index < first || index >= last)
                && index != requestedIndex
                && !realized[index].Control.IsKeyboardFocusWithin)
                recycle(index);
        }
        Size cellSize = new(ItemWidth, ItemHeight);
        for (int index = first; index < last; index++)
            realize(index).Measure(cellSize);
        if (requestedIndex >= 0 && requestedIndex < Items.Count)
            realize(requestedIndex).Measure(cellSize);
        return new Size(width, Math.Ceiling(Items.Count / (double)columns) * ItemHeight);
    }

    protected override Size ArrangeOverride(Size finalSize)
    {
        int finalColumns = Math.Max(1, (int)Math.Floor(finalSize.Width / ItemWidth));
        if (columns != finalColumns)
        {
            columns = finalColumns;
            InvalidateMeasure();
        }
        foreach (KeyValuePair<int, ContainerEntry> entry in realized)
            entry.Value.Control.Arrange(itemBounds(entry.Key));
        return finalSize;
    }

    protected override Control? ScrollIntoView(int index)
    {
        if (index < 0 || index >= Items.Count || ItemContainerGenerator is null)
            return null;
        requestedIndex = index;
        Control container = realize(index);
        container.Measure(new Size(ItemWidth, ItemHeight));
        container.Arrange(itemBounds(index));
        container.BringIntoView();
        InvalidateMeasure();
        Dispatcher.UIThread.Post(() =>
        {
            if (requestedIndex == index && realized.TryGetValue(index, out ContainerEntry? entry))
            {
                entry.Control.BringIntoView();
                requestedIndex = -1;
                InvalidateMeasure();
            }
        }, DispatcherPriority.Loaded);
        return container;
    }

    protected override Control? ContainerFromIndex(int index)
    {
        if (realized.TryGetValue(index, out ContainerEntry? entry))
            return entry.Control;
        return index >= 0 && index < Items.Count && Items[index] is Control control
            && ownContainers.Contains(control) ? control : null;
    }

    protected override int IndexFromContainer(Control container)
    {
        foreach (KeyValuePair<int, ContainerEntry> entry in realized)
        {
            if (entry.Value.Control == container)
                return entry.Key;
        }
        if (ownContainers.Contains(container))
        {
            for (int index = 0; index < Items.Count; index++)
            {
                if (ReferenceEquals(Items[index], container))
                    return index;
            }
        }
        return -1;
    }

    protected override IEnumerable<Control> GetRealizedContainers() =>
        realized.OrderBy(entry => entry.Key).Select(entry => entry.Value.Control);

    protected override IInputElement? GetControl(NavigationDirection direction, IInputElement? from, bool wrap)
    {
        if (Items.Count == 0)
            return null;
        Control? fromControl = from as Control;
        while (fromControl is not null && fromControl.GetVisualParent() != this)
            fromControl = fromControl.GetVisualParent() as Control;
        int start = fromControl is null ? -1 : IndexFromContainer(fromControl);
        int page = columns * Math.Max(1, (int)Math.Floor(viewportHeight / ItemHeight));
        int target = direction switch
        {
            NavigationDirection.First => 0,
            NavigationDirection.Last => Items.Count - 1,
            NavigationDirection.Next or NavigationDirection.Right => start + 1,
            NavigationDirection.Previous or NavigationDirection.Left => start - 1,
            NavigationDirection.Up => start - columns,
            NavigationDirection.Down => start < 0 ? 0 : start + columns,
            NavigationDirection.PageUp => Math.Max(0, start - page),
            NavigationDirection.PageDown => Math.Min(Items.Count - 1, start + page),
            _ => -1,
        };
        if (direction == NavigationDirection.Down && start >= 0 && target >= Items.Count
            && start / columns < (Items.Count - 1) / columns)
            target = Items.Count - 1;
        if (wrap)
            target = (target % Items.Count + Items.Count) % Items.Count;
        if (target < 0 || target >= Items.Count)
            return null;
        return ScrollIntoView(target);
    }

    protected override void OnItemsChanged(IReadOnlyList<object?> items, NotifyCollectionChangedEventArgs e)
    {
        int lastRealized = realized.Keys.DefaultIfEmpty(-1).Max();
        if (e.Action == NotifyCollectionChangedAction.Add && e.NewStartingIndex > lastRealized
            || e.Action == NotifyCollectionChangedAction.Remove && e.OldStartingIndex > lastRealized)
        {
            InvalidateMeasure();
            return;
        }
        foreach (int index in realized.Keys.ToArray())
            recycle(index, true);
        foreach (Control control in ownContainers.ToArray())
        {
            if (!items.Any(item => ReferenceEquals(item, control)))
            {
                if (Children.Contains(control))
                    RemoveInternalChild(control);
                ownContainers.Remove(control);
            }
        }
        requestedIndex = -1;
        InvalidateMeasure();
    }

    protected override void OnItemsControlChanged(ItemsControl? oldValue)
    {
        realized.Clear();
        recyclePool.Clear();
        ownContainers.Clear();
        requestedIndex = -1;
        viewport = default;
        base.OnItemsControlChanged(oldValue);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
    {
        if (ItemContainerGenerator is not null)
        {
            foreach (int index in realized.Keys.ToArray())
                recycle(index, true);
        }
        recyclePool.Clear();
        base.OnDetachedFromVisualTree(e);
    }

    private Control realize(int index)
    {
        if (realized.TryGetValue(index, out ContainerEntry? existing))
            return existing.Control;
        ItemContainerGenerator generator = ItemContainerGenerator!;
        object? item = Items[index];
        bool needsContainer = generator.NeedsContainer(item, index, out object? recycleKey);
        Control container = needsContainer
            ? recycleKey is not null && recyclePool.TryGetValue(recycleKey, out Stack<Control>? pool) && pool.Count > 0
                ? pool.Pop()
                : generator.CreateContainer(item, index, recycleKey)
            : (Control)item!;
        bool prepare = needsContainer || ownContainers.Add(container);
        realized.Add(index, new ContainerEntry(container, recycleKey, needsContainer));
        container.SetCurrentValue(IsVisibleProperty, true);
        if (prepare)
            generator.PrepareItemContainer(container, item, index);
        if (!Children.Contains(container))
            AddInternalChild(container);
        if (prepare)
            generator.ItemContainerPrepared(container, item, index);
        return container;
    }

    private void recycle(int index, bool removed = false)
    {
        ContainerEntry entry = realized[index];
        realized.Remove(index);
        if (!entry.NeedsContainer)
        {
            entry.Control.SetCurrentValue(IsVisibleProperty, false);
            if (removed && Children.Contains(entry.Control))
                RemoveInternalChild(entry.Control);
            return;
        }
        ItemContainerGenerator!.ClearItemContainer(entry.Control);
        entry.Control.SetCurrentValue(IsVisibleProperty, false);
        RemoveInternalChild(entry.Control);
        if (entry.RecycleKey is not null)
        {
            if (!recyclePool.TryGetValue(entry.RecycleKey, out Stack<Control>? pool))
            {
                pool = new Stack<Control>();
                recyclePool.Add(entry.RecycleKey, pool);
            }
            pool.Push(entry.Control);
        }
    }

    private Rect itemBounds(int index) =>
        new(index % columns * ItemWidth, index / columns * ItemHeight, ItemWidth, ItemHeight);

    private void onEffectiveViewportChanged(object? sender, EffectiveViewportChangedEventArgs e)
    {
        if (viewport == e.EffectiveViewport)
            return;
        viewport = e.EffectiveViewport;
        InvalidateMeasure();
    }

    private sealed record ContainerEntry(Control Control, object? RecycleKey, bool NeedsContainer);
}
