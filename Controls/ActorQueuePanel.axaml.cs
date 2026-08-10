using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels;
using System;

namespace Ludork.Controls;

public partial class ActorQueuePanel : UserControl
{
    private readonly DispatcherTimer clickTimer = new() { Interval = TimeSpan.FromMilliseconds(500) };
    private ActorQueueItemViewModel? pendingClickItem;
    private ActorQueueViewModel? previewViewModel;

    public ActorQueuePanel()
    {
        InitializeComponent();
        clickTimer.Tick += (_, _) => toggleSelection();
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
            previewViewModel.SetPreviewActive(false);
        previewViewModel = null;
        clickTimer.Stop();
        pendingClickItem = null;
        base.OnDetachedFromVisualTree(args);
    }

    private void updatePreviewActivity()
    {
        ActorQueueViewModel? next = DataContext as ActorQueueViewModel;
        if (previewViewModel != next)
            previewViewModel?.SetPreviewActive(false);
        previewViewModel = next;
        previewViewModel?.SetPreviewActive(IsEffectivelyVisible && VisualRoot is not null);
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
        MenuItem remove = new MenuItem { Header = LocaleService.Get("REMOVE_FROM_RECENTLY_PLACED") };
        remove.Click += (_, _) => viewModel.Remove(item, previousItem);
        MenuItem locate = new MenuItem { Header = LocaleService.Get("LOCATE_BLUEPRINT") };
        locate.Click += (_, _) => viewModel.RequestLocate(item);
        Queue.ContextMenu = new ContextMenu { ItemsSource = new[] { remove, locate } };
        Queue.ContextMenu.Open(Queue);
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
