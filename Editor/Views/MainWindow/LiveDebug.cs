using Avalonia.Controls;
using Avalonia.Threading;
using Ludork.Controls;
using Ludork.Models;
using Ludork.Services;
using System;
using System.Linq;

namespace Ludork.Views;

public partial class MainWindow
{
    private LiveDebugViewState? liveDebugViewState;

    private void onLiveDebugClicked(object? sender, EventArgs args)
    {
        if (viewModel?.MapWorkspace.CanConfigureLiveDebug == true)
            viewModel.MapWorkspace.LiveDebug = !viewModel.MapWorkspace.LiveDebug;
    }

    private void prepareLiveDebug()
    {
        if (viewModel?.MapWorkspace.LiveDebug != true)
            return;
        liveDebug?.Prepare();
        liveDebugViewState = new(viewModel.MapWorkspace.SelectedMap?.Key,
            viewModel.MapWorkspace.SelectedLayerTab is { IsOverview: false } layer ? layer.Name : null,
            EditorPanel.EditMode, EditorPanel.CaptureViewport(), WorldEditorPanel.CaptureViewport());
        EditorPanel.CancelInteractions();
    }

    private void onLiveDebugEditingContextChanged(object? sender, IMapEditingContext context)
    {
        EditorPanel.ConfigureEditingContext(context);
        ActorInfoPanel.ConfigureEditingContext(context);
    }

    private void onLiveDebugStarted(object? sender, EventArgs args)
    {
        refreshMapPanel();
        if (EditorPanel.EditMode == MapEditMode.Light)
            selectPreviewMode(MapEditMode.Tile);
    }

    private void onLiveDebugContextChanged(object? sender, EventArgs args) => EditorPanel.CancelInteractions();
    private void onLiveDebugStatusChanged(object? sender, EventArgs args) => updateLiveDebugControls();

    private void onLiveDebugError(object? sender, string error)
    {
        toast.ShowMessage(error, 4000);
        appendConsoleLine("[Live Debug] " + error);
    }

    private void updateLiveDebugControls()
    {
        if (viewModel is null)
            return;
        bool active = liveDebug?.IsActive == true;
        bool editable = liveDebug?.IsEditable == true;
        if (active)
        {
            EditModeToggles.IsEnabled = editable;
            TileModeToggle.IsEnabled = editable;
            ActorModeToggle.IsEnabled = editable;
            LightModeToggle.IsEnabled = false;
            EditorPanel.IsEnabled = editable;
            LayerTabs.IsEnabled = editable;
            RightList.IsEnabled = editable;
            ActorInfoPanel.IsEnabled = editable;
            RightModePanel.IsEnabled = editable;
            FileExplorerPanel.IsEnabled = true;
            ActorOutliner.IsEnabled = editable;
            MapList.IsEnabled = false;
            WorldEditorPanel.IsEnabled = false;
            refreshMapPanelState();
        }
        else
        {
            TileModeToggle.IsEnabled = true;
            ActorModeToggle.IsEnabled = true;
            LightModeToggle.IsEnabled = true;
        }
        LiveDebugBanner.IsVisible = active;
        LiveDebugStatus.Text = active
            ? LocaleService.Get(editable ? "LIVE_DEBUG_ACTIVE" : liveDebug!.Status switch
            {
                "disconnected" => "LIVE_DEBUG_DISCONNECTED",
                "unavailable" => "LIVE_DEBUG_UNAVAILABLE",
                "no_player" => "LIVE_DEBUG_NO_PLAYER",
                "world_hole" => "LIVE_DEBUG_WORLD_HOLE",
                "loading_map" or "waiting" => "LIVE_DEBUG_LOADING",
                "no_scene" => "LIVE_DEBUG_NO_SCENE",
                _ => "LIVE_DEBUG_NO_MAP",
            }) : string.Empty;
    }

    private void onLiveDebugEnded(object? sender, EventArgs args)
    {
        if (liveDebugViewState is not null && viewModel is not null)
        {
            LiveDebugViewState previous = liveDebugViewState;
            liveDebugViewState = null;
            viewModel.MapWorkspace.SelectedMap = viewModel.MapWorkspace.findMapItem(previous.MapKey);
            refreshMapPanel();
            viewModel.MapWorkspace.SelectedLayerTab = viewModel.MapWorkspace.LayerTabs.FirstOrDefault(
                layer => !layer.IsOverview && layer.Name == previous.LayerName) ?? viewModel.MapWorkspace.LayerTabs.FirstOrDefault();
            selectPreviewMode(previous.Mode);
            Dispatcher.UIThread.Post(() =>
            {
                EditorPanel.RestoreViewport(previous.Viewport);
                WorldEditorPanel.RestoreViewport(previous.WorldViewport);
            }, DispatcherPriority.Loaded);
        }
        updateLiveDebugControls();
    }

    private sealed record LiveDebugViewState(string? MapKey, string? LayerName,
        MapEditMode Mode, MapPanelViewportState Viewport, WorldMapViewportState WorldViewport);
}
