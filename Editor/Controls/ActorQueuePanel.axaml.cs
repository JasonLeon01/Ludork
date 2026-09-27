using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;
using System;
using System.Linq;

namespace Ludork.Controls;

public partial class ActorQueuePanel : UserControl
{
    private readonly DispatcherTimer clickTimer = new() { Interval = TimeSpan.FromMilliseconds(500) };
    private ActorQueueItemViewModel? pendingClickItem;
    private ActorQueueViewModel? previewViewModel;
    private bool previewsActive;
    private ListBox activeQueue => IconQueue.IsVisible ? IconQueue : ListQueue;

    public ActorQueuePanel()
    {
        InitializeComponent();
        clickTimer.Tick += (_, _) => toggleSelection();
        EditorInputs.ApplyEditable(SearchBox);
        SearchBox.PlaceholderText = LocaleService.Get("SEARCH_ACTORS");
        IconQueue.AddHandler(PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel);
        ListQueue.AddHandler(PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel);
        IconQueue.AddHandler(ContextRequestedEvent, onContextRequested, RoutingStrategies.Tunnel);
        ListQueue.AddHandler(ContextRequestedEvent, onContextRequested, RoutingStrategies.Tunnel);
        DataContextChanged += (_, _) => updateViewMode();
        ViewModeButton.PropertyChanged += (_, args) =>
        {
            if (args.Property == ToggleButton.IsCheckedProperty)
                updateViewMode();
        };
        EffectiveViewportChanged += (_, _) => updatePreviewActivity();
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        updateViewMode();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        if (previewViewModel is not null)
            previewViewModel.DeactivatePreviews();
        previewViewModel = null;
        previewsActive = false;
        clickTimer.Stop();
        pendingClickItem = null;
        base.OnDetachedFromVisualTree(args);
    }

    private void updatePreviewActivity()
    {
        ActorQueueViewModel? next = DataContext as ActorQueueViewModel;
        if (previewViewModel != next)
            previewViewModel?.DeactivatePreviews();
        previewViewModel = next;
        previewsActive = IsEffectivelyVisible && VisualRoot is not null;
        if (!previewsActive)
            previewViewModel?.DeactivatePreviews();
        foreach (ActorQueueItemControl row in this.GetVisualDescendants().OfType<ActorQueueItemControl>())
            row.RefreshPreviewActivity();
    }

    private void updateViewMode()
    {
        bool iconMode = ViewModeButton.IsChecked == true;
        ListBox previous = activeQueue;
        bool restoreFocus = previous.IsKeyboardFocusWithin;
        ActorQueueItemViewModel? selected = (DataContext as ActorQueueViewModel)?.SelectedItem;
        IconQueue.IsVisible = iconMode;
        ListQueue.IsVisible = !iconMode;
        if (previous != activeQueue)
        {
            clickTimer.Stop();
            pendingClickItem = null;
            activeQueue.SetCurrentValue(SelectingItemsControl.SelectedItemProperty, selected);
            if (selected is not null)
                activeQueue.ScrollIntoView(selected);
            if (restoreFocus)
                activeQueue.Focus();
        }
        updatePreviewActivity();
    }

    internal bool IsItemPreviewVisible(ActorQueueItemControl row)
    {
        ListBox? list = row.FindAncestorOfType<ListBox>();
        if (!previewsActive || !ReferenceEquals(list, activeQueue)
            || !row.IsEffectivelyVisible || row.Bounds.Width <= 0 || row.Bounds.Height <= 0)
            return false;
        Point? origin = row.TranslatePoint(default, activeQueue);
        if (origin is not Point position)
            return false;
        Rect rowBounds = new Rect(position, row.Bounds.Size);
        Rect viewport = new Rect(activeQueue.Bounds.Size);
        return rowBounds.Intersects(viewport);
    }

    private void onDoubleTapped(object? sender, TappedEventArgs args)
    {
        clickTimer.Stop();
        pendingClickItem = null;
        if (sender is ListBox list && !isFavoriteButton(args.Source))
            (DataContext as ActorQueueViewModel)?.RequestOpen(getItemAt(list, args.GetPosition(list)));
    }

    private void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (sender is not ListBox list || isFavoriteButton(args.Source))
            return;
        PointerPoint point = args.GetCurrentPoint(list);
        ActorQueueItemViewModel? item = getItemAt(list, point.Position);
        if (point.Properties.IsLeftButtonPressed && !point.Properties.IsRightButtonPressed)
            handleLeftClick(item);
    }

    private void onContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        if (sender is not ListBox list)
            return;
        ActorQueueItemViewModel? item = args.TryGetPosition(list, out Point position)
            ? getItemAt(list, position)
            : list.SelectedItem as ActorQueueItemViewModel;
        if (item is null || DataContext is not ActorQueueViewModel viewModel)
            return;

        ActorQueueItemViewModel? previousItem = viewModel.SelectedItem;
        clickTimer.Stop();
        pendingClickItem = null;
        viewModel.Select(item, true);
        MenuItem open = new MenuItem { Header = LocaleService.Get("OPEN_BLUEPRINT") };
        open.Click += (_, _) => viewModel.RequestOpen(item);
        MenuItem locate = new MenuItem { Header = LocaleService.Get("LOCATE_BLUEPRINT") };
        locate.Click += (_, _) => viewModel.RequestLocate(item);
        MenuItem favorite = new MenuItem
        {
            Header = LocaleService.Get(item.IsFavorite ? "REMOVE_FROM_FAVOURITES" : "ADD_TO_FAVOURITES"),
        };
        favorite.Click += (_, _) => viewModel.ToggleFavorite(item);
        MenuItem remove = new MenuItem { Header = LocaleService.Get("REMOVE_FROM_RECENTLY_PLACED") };
        remove.Click += (_, _) => viewModel.RemoveRecent(item, previousItem);
        ContextMenu menu = new()
        {
            ItemsSource = item.IsRecent
                ? new[] { open, locate, favorite, remove }
                : new[] { open, locate, favorite },
        };
        menu.Open(list);
        args.Handled = true;
    }

    private void onFavouriteClick(object? sender, RoutedEventArgs args)
    {
        clickTimer.Stop();
        pendingClickItem = null;
        if (sender is not ToggleButton { DataContext: ActorQueueItemViewModel item }
            || DataContext is not ActorQueueViewModel viewModel)
        {
            return;
        }
        viewModel.ToggleFavorite(item);
        args.Handled = true;
    }

    private void handleLeftClick(ActorQueueItemViewModel? item)
    {
        clickTimer.Stop();
        pendingClickItem = null;
        if (item is not null && item == activeQueue.SelectedItem)
        {
            pendingClickItem = item;
            clickTimer.Start();
        }
    }

    private void toggleSelection()
    {
        clickTimer.Stop();
        if (pendingClickItem is not null && pendingClickItem == activeQueue.SelectedItem)
            (DataContext as ActorQueueViewModel)?.Select(null);
        pendingClickItem = null;
    }

    private static bool isFavoriteButton(object? source)
    {
        return source is ToggleButton
            || source is Visual visual && visual.GetVisualAncestors().OfType<ToggleButton>().Any();
    }

    private static ActorQueueItemViewModel? getItemAt(ListBox list, Point position)
    {
        Visual? visual = list.InputHitTest(position) as Visual;
        while (visual is not null)
        {
            if (visual is ListBoxItem { DataContext: ActorQueueItemViewModel item })
                return item;
            visual = visual.GetVisualParent();
        }
        return null;
    }
}
