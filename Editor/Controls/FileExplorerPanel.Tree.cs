using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Selection;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.ViewModels;
using System.Linq;

namespace Ludork.Controls;

public partial class FileExplorerPanel
{
    private static ToggleButton? getDirectoryExpander(object? source)
    {
        if (source is ToggleButton button && button.Classes.Contains("directoryExpander"))
            return button;
        return (source as Avalonia.Visual)?.GetVisualAncestors().OfType<ToggleButton>()
            .FirstOrDefault(candidate => candidate.Classes.Contains("directoryExpander"));
    }

    private async void onDirectoryToggleClick(object? sender, RoutedEventArgs args)
    {
        args.Handled = true;
        if (sender is ToggleButton { DataContext: FileExplorerEntryViewModel entry }
            && DataContext is FileExplorerViewModel viewModel)
            await viewModel.ToggleDirectoryAsync(entry);
    }

    private async void onTreeKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Key is not (Key.Left or Key.Right) || args.KeyModifiers != KeyModifiers.None
            || DataContext is not FileExplorerViewModel { IconView: false, IsSearching: false } viewModel
            || ListEntries.SelectedItem is not FileExplorerEntryViewModel entry)
            return;
        args.Handled = true;
        if (args.Key == Key.Right)
        {
            if (!entry.CanExpand)
                return;
            if (!entry.IsExpanded)
            {
                await viewModel.ToggleDirectoryAsync(entry);
                return;
            }
            int childIndex = viewModel.Entries.IndexOf(entry) + 1;
            if (childIndex > 0 && childIndex < viewModel.Entries.Count
                && viewModel.GetParentEntry(viewModel.Entries[childIndex]) == entry)
                selectTreeEntry(viewModel.Entries[childIndex]);
        }
        else if (entry.CanExpand && entry.IsExpanded)
            await viewModel.ToggleDirectoryAsync(entry);
        else if (viewModel.GetParentEntry(entry) is FileExplorerEntryViewModel parent)
            selectTreeEntry(parent);
    }

    private void selectTreeEntry(FileExplorerEntryViewModel entry)
    {
        int index = ListEntries.Items.IndexOf(entry);
        if (index < 0)
            return;
        using (ListEntries.Selection.BatchUpdate())
        {
            ListEntries.Selection.Clear();
            ListEntries.Selection.Select(index);
        }
        ListEntries.ScrollIntoView(entry);
        ListEntries.UpdateLayout();
        ListEntries.ContainerFromItem(entry)?.Focus();
    }
}
