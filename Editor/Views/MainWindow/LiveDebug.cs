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
    private LiveDebugSession? liveDebugSession;
    private LiveDebugViewState? liveDebugViewState;
    private bool liveDebugRequested;
    private string? liveDebugContext;

    private void onLiveDebugClicked(object? sender, EventArgs args)
    {
        if (viewModel?.CanConfigureLiveDebug == true)
            viewModel.LiveDebug = !viewModel.LiveDebug;
    }

    private void prepareLiveDebug()
    {
        if (viewModel?.LiveDebug != true)
            return;
        liveDebugRequested = true;
        liveDebugViewState = new(viewModel.SelectedMap?.Key,
            viewModel.SelectedLayerTab is { IsOverview: false } layer ? layer.Name : null,
            EditorPanel.EditMode, EditorPanel.CaptureViewport(), WorldEditorPanel.CaptureViewport());
        EditorPanel.CancelInteractions();
    }

    private void updateLiveDebugRunState(ProjectRunState state)
    {
        if (state == ProjectRunState.Idle)
        {
            endLiveDebug();
            return;
        }
        if (state != ProjectRunState.Running || !liveDebugRequested
            || liveDebugSession is not null || projectRunner is null || viewModel is null)
            return;
        liveDebugSession = new LiveDebugSession(projectRunner);
        viewModel.SetLiveDebugSession(liveDebugSession);
        EditorPanel.ConfigureEditingContext(liveDebugSession);
        ActorInfoPanel.ConfigureEditingContext(liveDebugSession);
        liveDebugSession.StateChanged += onLiveDebugStateChanged;
        liveDebugSession.ErrorReceived += onLiveDebugError;
        refreshMapPanel();
        if (EditorPanel.EditMode == MapEditMode.Light)
            selectPreviewMode(MapEditMode.Tile);
        liveDebugSession.SetConnection(projectRunner.CanSendCommand);
    }

    private void onLiveDebugError(object? sender, string error)
    {
        toast.ShowMessage(error, 4000);
        appendConsoleLine("[Live Debug] " + error);
    }

    private void onLiveDebugStateChanged(object? sender, EventArgs args)
    {
        if (liveDebugSession is null || viewModel is null)
            return;
        if (liveDebugContext != liveDebugSession.Context)
        {
            liveDebugContext = liveDebugSession.Context;
            EditorPanel.CancelInteractions();
        }
        viewModel.RefreshLiveDebugState();
        updateLiveDebugControls();
    }

    private void updateLiveDebugControls()
    {
        if (viewModel is null)
            return;
        bool active = liveDebugSession is not null;
        bool editable = liveDebugSession?.IsEditable == true;
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
            ? LocaleService.Get(editable ? "LIVE_DEBUG_ACTIVE" : liveDebugSession!.Status switch
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

    private void endLiveDebug()
    {
        if (liveDebugSession is not null)
        {
            liveDebugSession.StateChanged -= onLiveDebugStateChanged;
            liveDebugSession.ErrorReceived -= onLiveDebugError;
            liveDebugSession.Dispose();
            liveDebugSession = null;
            if (viewModel is not null)
            {
                viewModel.SetLiveDebugSession(null);
                ProjectMapEditingContext context = new(viewModel.GameData);
                EditorPanel.ConfigureEditingContext(context);
                ActorInfoPanel.ConfigureEditingContext(context);
            }
        }
        liveDebugRequested = false;
        liveDebugContext = null;
        if (liveDebugViewState is not null && viewModel is not null)
        {
            LiveDebugViewState previous = liveDebugViewState;
            liveDebugViewState = null;
            viewModel.SelectedMap = viewModel.findMapItem(previous.MapKey);
            refreshMapPanel();
            viewModel.SelectedLayerTab = viewModel.LayerTabs.FirstOrDefault(
                layer => !layer.IsOverview && layer.Name == previous.LayerName) ?? viewModel.LayerTabs.FirstOrDefault();
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
