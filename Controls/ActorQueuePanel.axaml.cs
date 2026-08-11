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

    public ActorQueuePanel()
    {
        InitializeComponent();
        clickTimer.Tick += (_, _) => toggleSelection();
        EditorInputs.ApplyEditable(SearchBox);
        SearchBox.PlaceholderText = LocaleService.Get("SEARCH_ACTORS");
        Queue.AddHandler(PointerPressedEvent, onPointerPressed, RoutingStrategies.Tunnel);
        DataContextChanged += (_, _) => updatePreviewActivity();
        EffectiveViewportChanged += (_, _) => updatePreviewActivity();
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        updatePreviewActivity();
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
        foreach (ActorQueueItemControl row in Queue.GetVisualDescendants().OfType<ActorQueueItemControl>())
            row.RefreshPreviewActivity();
    }

    internal bool IsItemPreviewVisible(ActorQueueItemControl row)
    {
        if (!previewsActive || !row.IsEffectivelyVisible || row.Bounds.Width <= 0 || row.Bounds.Height <= 0)
            return false;
        Point? origin = row.TranslatePoint(default, Queue);
        if (origin is not Point position)
            return false;
        Rect rowBounds = new Rect(position, row.Bounds.Size);
        Rect viewport = new Rect(Queue.Bounds.Size);
        return rowBounds.Intersects(viewport);
    }

    private void onDoubleTapped(object? sender, TappedEventArgs args)
    {
        clickTimer.Stop();
        pendingClickItem = null;
        (DataContext as ActorQueueViewModel)?.RequestOpen(Queue.SelectedItem as ActorQueueItemViewModel);
    }

    private void onPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        PointerPoint point = args.GetCurrentPoint(Queue);
        ActorQueueItemViewModel? item = getItemAt(point.Position);
        if (point.Properties.IsLeftButtonPressed)
        {
            handleLeftClick(item);
            return;
        }
        if (!point.Properties.IsRightButtonPressed || item is null || DataContext is not ActorQueueViewModel viewModel)
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
        Queue.ContextMenu = new ContextMenu
        {
            ItemsSource = item.IsRecent
                ? new[] { open, locate, favorite, remove }
                : new[] { open, locate, favorite },
        };
        Queue.ContextMenu.Open(Queue);
        args.Handled = true;
    }

    private void onFavouriteClick(object? sender, RoutedEventArgs args)
    {
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
        if (item is not null && item == Queue.SelectedItem)
        {
            pendingClickItem = item;
            clickTimer.Start();
        }
    }

    private void toggleSelection()
    {
        clickTimer.Stop();
        if (pendingClickItem is not null && pendingClickItem == Queue.SelectedItem)
            (DataContext as ActorQueueViewModel)?.Select(null);
        pendingClickItem = null;
    }

    private ActorQueueItemViewModel? getItemAt(Avalonia.Point position)
    {
        Visual? visual = Queue.InputHitTest(position) as Visual;
        while (visual is not null)
        {
            if (visual is ListBoxItem { DataContext: ActorQueueItemViewModel item })
                return item;
            visual = visual.GetVisualParent();
        }
        return null;
    }
}
