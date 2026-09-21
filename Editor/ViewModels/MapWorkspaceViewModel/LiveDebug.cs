using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public sealed partial class MapWorkspaceViewModel
{
    private LiveDebugSession? liveDebugSession;
    private bool selectingRuntimeMap;
    private bool restoringRuntimeTileset;

    public bool LiveDebug
    {
        get => ProjectConfig.LiveDebug;
        set
        {
            if (!CanConfigureLiveDebug || ProjectConfig.LiveDebug == value)
                return;
            ProjectConfig.LiveDebug = value;
            OnPropertyChanged();
        }
    }

    public bool CanConfigureLiveDebug => CanEdit && ProjectConfig.IndividualWindow;
    public string LiveDebugLabel => LocaleService.Get("LIVE_DEBUG");
    public bool CanUseMapTools => CanEdit || liveDebugSession?.IsEditable == true;
    public bool IsLiveDebugActive => liveDebugSession is not null;

    public void SetLiveDebugSession(LiveDebugSession? session)
    {
        if (liveDebugSession is not null)
            liveDebugSession.Changed -= onRuntimeMapChanged;
        liveDebugSession = session;
        selectedMapSnapshot = null;
        selectedSnapshotKey = null;
        selectedMapDocument = null;
        selectedDocumentKey = null;
        if (session is not null)
            session.Changed += onRuntimeMapChanged;
        RefreshLiveDebugState();
    }

    public void RefreshLiveDebugState()
    {
        OnPropertyChanged(nameof(CanUseMapTools));
        OnPropertyChanged(nameof(IsLiveDebugActive));
        if (liveDebugSession is not null && liveDebugSession.MapKey != SelectedMap?.Key)
        {
            selectingRuntimeMap = true;
            SelectedMap = findMapItem(liveDebugSession.MapKey);
            selectingRuntimeMap = false;
        }
    }

    private void onRuntimeMapChanged(object? sender, MapPreviewChangedEventArgs args)
    {
        onMapPreviewChanged(sender, args);
        if (args.Edit is null)
            refreshLayerTabs();
    }

    private IEnumerable<string> displayedLayerNames()
    {
        if (liveDebugSession is null)
            return SelectedMap is null ? [] : GameData.Maps.getLayerNames(SelectedMap.Key);
        return SelectedMapDocument?.LayerOrder ?? [];
    }

    private void restoreRuntimeTileset()
    {
        if (restoringRuntimeTileset || liveDebugSession is null
            || SelectedLayerTab is not { IsOverview: false } layer)
            return;
        restoringRuntimeTileset = true;
        TileSelect.setCurrentTilesetKey(SelectedMapDocument?.Layers.GetValueOrDefault(layer.Name)?.Tileset);
        restoringRuntimeTileset = false;
    }
}
