using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Services;
using Ludork.ViewModels;
using System.Collections.Generic;
using System.Linq;

namespace Ludork.Views;

public partial class MainWindow
{
    private void syncActorOutlinerSelection()
    {
        if (viewModel is null)
            return;
        List<ActorOutlinerItemViewModel> selection = [];
        ActorOutlinerItemViewModel[] items = viewModel.MapWorkspace.ActorOutlinerItems
            .SelectMany(item => item.EnumerateDescendants()).ToArray();
        if (EditorPanel.IsRuntimeEditing && EditorPanel.SelectedRuntimeActorId is string runtimeId)
        {
            if (items.FirstOrDefault(item => item.RuntimeId == runtimeId) is ActorOutlinerItemViewModel runtimeItem)
                selection.Add(runtimeItem);
        }
        else
        {
            foreach (MapActorReference actor in EditorPanel.SelectedActors)
                if (items.FirstOrDefault(item => item.LayerName == actor.LayerName && item.ActorTag == actor.Tag)
                    is ActorOutlinerItemViewModel item)
                    selection.Add(item);
        }
        if (selection.Count == 0 && viewModel.MapWorkspace.SelectedLayerTab is { IsOverview: false } layer
            && viewModel.MapWorkspace.ActorOutlinerItems.FirstOrDefault(item => item.LayerName == layer.Name)
                is ActorOutlinerItemViewModel layerItem)
            selection.Add(layerItem);
        updatingActorOutlinerSelection = true;
        ActorOutliner.SelectionMode = EditorPanel.IsRuntimeEditing ? SelectionMode.Single : SelectionMode.Multiple;
        foreach (object previous in ActorOutliner.SelectedItems.Cast<object>().ToArray())
            if (!selection.Contains(previous))
                ActorOutliner.SelectedItems.Remove(previous);
        foreach (ActorOutlinerItemViewModel item in selection)
            if (!ActorOutliner.SelectedItems.Contains(item))
                ActorOutliner.SelectedItems.Add(item);
        updatingActorOutlinerSelection = false;
    }

    private void onActorOutlinerSelectionChanged(object? sender, SelectionChangedEventArgs args)
    {
        if (updatingActorOutlinerSelection || viewModel is null || viewModel.MapWorkspace.IsRefreshingActorOutliner)
            return;
        ActorOutlinerItemViewModel? item = args.AddedItems.OfType<ActorOutlinerItemViewModel>().LastOrDefault()
            ?? ActorOutliner.SelectedItem as ActorOutlinerItemViewModel;
        if (item is null)
        {
            if (!EditorPanel.IsRuntimeEditing)
                EditorPanel.SelectActors([], null, null);
            return;
        }
        bool sameLayer = viewModel.MapWorkspace.SelectedLayerTab?.Name == item.LayerName;
        if (!selectOutlinerLayer(item))
            return;
        if (EditorPanel.IsRuntimeEditing)
        {
            EditorPanel.selectActor(item.LayerName, item.ActorIndex);
            return;
        }
        MapActorReference? primary = outlinerActorReference(item);
        MapActorReference[] actors = !sameLayer || primary is null ? primary is null ? [] : [primary]
            : ActorOutliner.SelectedItems.OfType<ActorOutlinerItemViewModel>()
                .Where(selected => selected.LayerName == item.LayerName)
                .Select(outlinerActorReference).OfType<MapActorReference>().ToArray();
        EditorPanel.SelectActors(actors, primary, EditorPanel.ActorSelectionAnchor ?? primary);
    }

    private static MapActorReference? outlinerActorReference(ActorOutlinerItemViewModel item)
        => item.ActorIndex is not null && item.ActorTag is string tag ? new(item.LayerName, tag) : null;

    private bool selectOutlinerLayer(ActorOutlinerItemViewModel item)
    {
        LayerTabViewModel? layer = viewModel?.MapWorkspace.LayerTabs.FirstOrDefault(tab => !tab.IsOverview && tab.Name == item.LayerName);
        if (layer is null || viewModel is null)
            return false;
        viewModel.MapWorkspace.SelectedLayerTab = layer;
        return true;
    }

    private static TreeViewItem? getActorOutlinerContainer(object? source)
        => source is TreeViewItem item ? item : (source as Visual)?.FindAncestorOfType<TreeViewItem>();

    private void onActorOutlinerPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (EditorPanel.IsRuntimeEditing || viewModel is null || !ActorOutliner.IsEnabled
            || args.Source is Visual source && source.GetVisualAncestors().OfType<ToggleButton>().Any())
            return;
        TreeViewItem? container = getActorOutlinerContainer(args.Source);
        if (container?.DataContext is not ActorOutlinerItemViewModel item)
            return;
        PointerPoint point = args.GetCurrentPoint(ActorOutliner);
        bool right = point.Properties.IsRightButtonPressed;
        if (!point.Properties.IsLeftButtonPressed && !right)
            return;
        bool sameLayer = viewModel.MapWorkspace.SelectedLayerTab?.Name == item.LayerName;
        if (!selectOutlinerLayer(item))
            return;
        MapActorReference? reference = outlinerActorReference(item);
        if (reference is null)
            EditorPanel.SelectActors([], null, null);
        else if (right)
        {
            if (!EditorPanel.SelectedActors.Contains(reference))
                EditorPanel.SelectActors([reference], reference, reference);
        }
        else if (!sameLayer)
            EditorPanel.SelectActors([reference], reference, reference);
        else if (args.KeyModifiers.HasFlag(KeyModifiers.Shift))
        {
            ActorOutlinerItemViewModel[] siblings = viewModel.MapWorkspace.ActorOutlinerItems
                .First(layer => layer.LayerName == item.LayerName).Children.ToArray();
            int anchor = System.Array.FindIndex(siblings, sibling => outlinerActorReference(sibling) == EditorPanel.ActorSelectionAnchor);
            int target = System.Array.IndexOf(siblings, item);
            if (anchor < 0)
                anchor = target;
            IEnumerable<MapActorReference> range = siblings.Skip(System.Math.Min(anchor, target))
                .Take(System.Math.Abs(target - anchor) + 1).Select(outlinerActorReference).OfType<MapActorReference>();
            MapActorReference[] actors = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers)
                ? EditorPanel.SelectedActors.Concat(range).Distinct().ToArray() : range.ToArray();
            EditorPanel.SelectActors(actors, reference, outlinerActorReference(siblings[anchor]));
        }
        else if (EditorShortcuts.HasPrimaryModifier(args.KeyModifiers))
        {
            MapActorReference[] actors = EditorPanel.SelectedActors.Contains(reference)
                ? EditorPanel.SelectedActors.Where(actor => actor != reference).ToArray()
                : EditorPanel.SelectedActors.Append(reference).ToArray();
            EditorPanel.SelectActors(actors, reference, reference);
        }
        else
            EditorPanel.SelectActors([reference], reference, reference);
        container.Focus();
        syncActorOutlinerSelection();
        beginTutorialOutlinerSelection(args, reference);
        args.Handled = true;
    }

    private void onActorOutlinerContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        if (!ActorOutliner.IsEnabled)
            return;
        MenuItem copy = new() { Header = LocaleService.Get("COPY"), IsEnabled = EditorPanel.SelectedActorIndex is not null };
        copy.Click += (_, _) => EditorPanel.CopySelectedActors();
        MenuItem delete = new()
        {
            Header = LocaleService.Get("DELETE"),
            IsEnabled = EditorPanel.SelectedActorIndex is not null && EditorPanel.IsSelectedLayerEditable,
        };
        delete.Click += (_, _) => EditorPanel.DeleteSelectedActors();
        ContextMenu menu = new() { ItemsSource = new object[] { copy, delete } };
        menu.Open(ActorOutliner);
        args.Handled = true;
    }

    private void onActorOutlinerKeyDown(object? sender, KeyEventArgs args)
    {
        if (args.Handled)
            return;
        if (EditorShortcuts.HasPrimaryModifier(args.KeyModifiers) && args.Key == Key.C)
        {
            EditorPanel.CopySelectedActors();
            args.Handled = true;
        }
        else if (args.Key is Key.Delete or Key.Back)
        {
            EditorPanel.DeleteSelectedActors();
            args.Handled = true;
        }
    }
}
