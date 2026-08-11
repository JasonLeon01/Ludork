using Avalonia;
using Avalonia.Controls;
using Avalonia.VisualTree;
using Ludork.ViewModels;

namespace Ludork.Controls;

public sealed class ActorQueueItemControl : ContentControl
{
    private ActorQueueItemViewModel? item;

    public ActorQueueItemControl()
    {
        DataContextChanged += (_, _) => updateItem();
        EffectiveViewportChanged += (_, _) => refreshPreviewActivity();
        PropertyChanged += (_, args) =>
        {
            if (args.Property == BoundsProperty || args.Property == IsVisibleProperty)
                refreshPreviewActivity();
        };
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        updateItem();
        refreshPreviewActivity();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        if (item is not null)
            item.IsPreviewActive = false;
        item = null;
        base.OnDetachedFromVisualTree(args);
    }

    internal void RefreshPreviewActivity()
    {
        refreshPreviewActivity();
    }

    private void updateItem()
    {
        ActorQueueItemViewModel? next = DataContext as ActorQueueItemViewModel;
        if (item != next)
        {
            if (item is not null)
                item.IsPreviewActive = false;
        }
        item = next;
        refreshPreviewActivity();
    }

    private void refreshPreviewActivity()
    {
        ActorQueuePanel? panel = this.FindAncestorOfType<ActorQueuePanel>();
        if (item is not null)
            item.IsPreviewActive = panel?.IsItemPreviewVisible(this) == true;
    }
}
