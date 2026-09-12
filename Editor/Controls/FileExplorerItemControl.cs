using Avalonia;
using Avalonia.Controls;
using Avalonia.VisualTree;
using Ludork.ViewModels;

namespace Ludork.Controls;

public sealed class FileExplorerItemControl : ContentControl
{
    private FileExplorerEntryViewModel? item;

    public FileExplorerItemControl()
    {
        DataContextChanged += (_, _) => updateItem();
        EffectiveViewportChanged += (_, _) => RefreshPreviewActivity();
        PropertyChanged += (_, args) =>
        {
            if (args.Property == BoundsProperty || args.Property == IsVisibleProperty)
                RefreshPreviewActivity();
        };
    }

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        updateItem();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        Deactivate();
        item = null;
        base.OnDetachedFromVisualTree(args);
    }

    internal void Deactivate() => item?.SetPresentation(this, false, false, 0);

    internal void RefreshPreviewActivity()
    {
        if (item is not null)
            this.FindAncestorOfType<FileExplorerPanel>()?.UpdateItemPresentation(this, item);
    }

    private void updateItem()
    {
        FileExplorerEntryViewModel? next = DataContext as FileExplorerEntryViewModel;
        if (!ReferenceEquals(next, item))
            Deactivate();
        item = next;
        RefreshPreviewActivity();
    }
}
