using Avalonia;
using Avalonia.Controls;
using Avalonia.VisualTree;
using Ludork.ViewModels;
using System;

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
            item.SetPreviewActive(this, false, 48);
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
                item.SetPreviewActive(this, false, 48);
        }
        item = next;
        refreshPreviewActivity();
    }

    private void refreshPreviewActivity()
    {
        ActorQueuePanel? panel = this.FindAncestorOfType<ActorQueuePanel>();
        if (item is not null)
        {
            int size = (int)Math.Ceiling(48 * (TopLevel.GetTopLevel(this)?.RenderScaling ?? 1));
            item.SetPreviewActive(this, panel?.IsItemPreviewVisible(this) == true, size);
        }
    }
}
