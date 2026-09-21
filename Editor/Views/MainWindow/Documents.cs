using System.Linq;
using System;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.ViewModels;
using Ludork.Views.Utils;

namespace Ludork.Views;

public partial class MainWindow
{
    private void onHistoryContextFocus(object? sender, FocusChangedEventArgs args)
    {
        if (viewModel?.CanEdit != true)
            return;
        updateHistoryContext(args.Source, false);
        if (viewModel?.ActiveDocument is not null && args.Source is Control control)
        {
            NumericUpDown? number = control as NumericUpDown ?? control.FindAncestorOfType<NumericUpDown>();
            if (number is not null)
                HistoryMergeBehavior.AttachFocused(number, viewModel.GameData);
            else if (control is TextBox text)
                HistoryMergeBehavior.AttachFocused(text, viewModel.GameData);
        }
    }

    private void onHistoryContextPointer(object? sender, PointerPressedEventArgs args)
    {
        updateHistoryContext(args.Source, true);
    }

    private void updateHistoryContext(object? source, bool pointer)
    {
        if (viewModel?.CanEdit != true)
            return;
        if (viewModel is null || source is not Avalonia.Visual visual)
            return;
        Control? control = visual as Control;
        if (control is MenuItem || visual.GetVisualAncestors().Any(ancestor => ancestor is MenuItem or Menu))
            return;
        if (isInsideHistoryControl(visual, FileExplorerPanel))
        {
            FileExplorerEntryViewModel? entry = control?.DataContext as FileExplorerEntryViewModel
                ?? visual.GetVisualAncestors().OfType<Control>()
                    .Select(ancestor => ancestor.DataContext).OfType<FileExplorerEntryViewModel>().FirstOrDefault();
            if (!pointer)
                entry ??= viewModel.FileExplorerPanel.SelectedEntry;
            viewModel.SetActiveDocument(entry is { IsDirectory: false }
                ? viewModel.GameData.GetDocumentByPath(entry.FullPath) : null);
            return;
        }
        if (isInsideHistoryControl(visual, MapList))
        {
            MapListItemViewModel? item = control?.DataContext as MapListItemViewModel
                ?? visual.GetVisualAncestors().OfType<Control>()
                    .Select(ancestor => ancestor.DataContext).OfType<MapListItemViewModel>().FirstOrDefault();
            if (!pointer)
                item ??= viewModel.MapWorkspace.SelectedMap;
            viewModel.SetActiveDocument(item is null ? null
                : viewModel.GameData.GetDocument(item.IsWorld ? "WorldMaps" : "Maps", item.Key));
            return;
        }
        bool mapContext = isInsideHistoryControl(visual, EditorPanel)
            || isInsideHistoryControl(visual, WorldEditorPanel)
            || isInsideHistoryControl(visual, LayerTabs)
            || isInsideHistoryControl(visual, ActorInfoPanel)
            || isInsideHistoryControl(visual, ActorOutliner)
            || isInsideHistoryControl(visual, EditModeToggles)
            || isInsideHistoryControl(visual, LightInfoPanel)
            || isInsideHistoryControl(visual, RightList);
        MapListItemViewModel? selected = viewModel.MapWorkspace.SelectedMap;
        viewModel.SetActiveDocument(mapContext && selected is not null
            ? viewModel.GameData.GetDocument(selected.IsWorld ? "WorldMaps" : "Maps", selected.Key) : null);
    }

    private static bool isInsideHistoryControl(Avalonia.Visual source, Control target)
    {
        return ReferenceEquals(source, target) || source.GetVisualAncestors().Any(ancestor => ReferenceEquals(ancestor, target));
    }
}
