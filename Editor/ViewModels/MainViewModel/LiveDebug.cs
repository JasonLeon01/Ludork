using Ludork.Models;
using Ludork.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.ViewModels;

public partial class MainViewModel
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

    public bool CanConfigureLiveDebug => CanEdit && IndividualWindow;
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
        if (session is not null)
            session.Changed += onRuntimeMapChanged;
        RefreshLiveDebugState();
    }

    public void RefreshLiveDebugState()
    {
        OnPropertyChanged(nameof(CanUseMapTools));
        OnPropertyChanged(nameof(IsLiveDebugActive));
        TileModeCommand.NotifyCanExecuteChanged();
        ActorModeCommand.NotifyCanExecuteChanged();
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
            return SelectedMap is null ? [] : GameData.getLayerNames(SelectedMap.Key);
        return SelectedMapData?["layerOrder"] is JsonArray order
            ? order.OfType<JsonValue>().Select(value => value.GetValue<string>()).ToArray() : [];
    }

    private void restoreRuntimeTileset()
    {
        if (restoringRuntimeTileset || liveDebugSession is null
            || SelectedLayerTab is not { IsOverview: false } layer)
            return;
        restoringRuntimeTileset = true;
        TileSelect.setCurrentTilesetKey(SelectedMapData?["layers"]?[layer.Name]?["layerTileset"]?.GetValue<string>());
        restoringRuntimeTileset = false;
    }
}
