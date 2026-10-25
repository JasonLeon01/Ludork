using Avalonia.Controls;
using Avalonia.Media;
using Ludork.Plugin.Abstractions;
using Ludork.Plugin.Avalonia;
using Ludork.Plugins.OfficialMotaMapFill.Runtime;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialMotaMapFill.UI;

internal sealed partial class MotaMapFillWindow : Window
{
    private readonly IMapEditorHost host;
    private readonly PluginLocalizer localizer;
    private readonly MotaRuntimeClient runtime;
    private readonly CancellationTokenSource cancellation;
    private readonly PluginMapCanvas mapCanvas;
    private readonly PluginMapCanvas blueprintCanvas;
    private readonly ListBox mapList = new();
    private readonly ComboBox layerSelector = new();
    private readonly ComboBox playerSelector = new();
    private readonly TextBox savePath = EditorInputs.CreateReadOnlyTextBox();
    private readonly Button chooseBlueprints = new();
    private readonly TextBlock attributes = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock regionText = new();
    private readonly TextBlock status = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock counts = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TextBlock blueprintLabel = new() { TextWrapping = TextWrapping.Wrap };
    private readonly TabControl categories = new();
    private readonly Button browse = new();
    private readonly Button reload = new();
    private readonly Button next = new();
    private readonly Button back = new();
    private readonly Button generate = new();
    private readonly Button fullMap = new();
    private readonly ProgressBar progress = new() { Height = 5, IsIndeterminate = true, IsVisible = false };
    private readonly HashSet<string> selected = new(StringComparer.Ordinal);
    private IReadOnlyList<PluginActorBlueprintSummary> blueprints = Array.Empty<PluginActorBlueprintSummary>();
    private MotaRuntimeResult? evaluation;
    private PluginMapSnapshot? currentMap;
    private PluginMapLayerSnapshot? currentLayer;
    private MapRegion region;
    private bool refreshing;
    private bool choosingActors;
    private bool busy;
    private bool closed;

    public MotaMapFillWindow(IMapEditorHost host, string suggestedMapKey, string pluginDirectory,
        PluginLocalizer localizer, CancellationToken cancellationToken)
    {
        this.host = host;
        this.localizer = localizer;
        runtime = new MotaRuntimeClient(host, pluginDirectory);
        cancellation = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        mapCanvas = new PluginMapCanvas(host);
        blueprintCanvas = new PluginMapCanvas(host) { MinWidth = 96, MinHeight = 96, Width = 96, Height = 96 };
        Title = localizer.Text("windowTitle");
        Width = 1320;
        Height = 840;
        MinWidth = 1020;
        MinHeight = 660;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = PluginTheme.Brush("Background");
        configureControls();
        Content = createLayout();
        populateMaps(suggestedMapKey);
        showStep(false);
        Closed += (_, _) =>
        {
            closed = true;
            cancellation.Cancel();
            mapCanvas.Dispose();
            blueprintCanvas.Dispose();
            if (!busy)
                cancellation.Dispose();
        };
    }

    private void configureControls()
    {
        browse.Content = localizer.Text("chooseSave");
        reload.Content = localizer.Text("reload");
        next.Content = localizer.Text("next");
        back.Content = localizer.Text("back");
        generate.Content = localizer.Text("generate");
        fullMap.Content = localizer.Text("fullMap");
        chooseBlueprints.Content = localizer.Text("chooseBlueprints");
        status.Text = localizer.Text("ready");
        browse.Click += async (_, _) => await runAsync(chooseSaveAsync);
        chooseBlueprints.Click += async (_, _) => await runAsync(chooseBlueprintsAsync);
        reload.Click += async (_, _) => await runAsync(loadSaveAsync);
        next.Click += (_, _) => showStep(true);
        back.Click += (_, _) => showStep(false);
        generate.Click += async (_, _) => await runAsync(generateAsync);
        fullMap.Click += (_, _) => setRegion(fullRegion());
        mapCanvas.RegionChanged += (_, args) => setRegion(args.Region);
        mapList.SelectionChanged += (_, _) => onMapChanged();
        layerSelector.SelectionChanged += (_, _) => onLayerChanged();
        playerSelector.SelectionChanged += async (_, _) =>
        {
            if (!refreshing && !busy)
                await runAsync(evaluatePlayerAsync);
        };
    }

    private void populateMaps(string suggestedMapKey)
    {
        refreshing = true;
        foreach (PluginMapSummary map in host.ListMaps())
            mapList.Items.Add(new ListBoxItem { Content = map.DisplayName, Tag = map.Key });
        mapList.SelectedItem = mapList.Items.OfType<ListBoxItem>().FirstOrDefault(item =>
            string.Equals(item.Tag as string, suggestedMapKey, StringComparison.Ordinal))
            ?? mapList.Items.OfType<ListBoxItem>().FirstOrDefault();
        refreshing = false;
        onMapChanged();
    }

    private void onMapChanged()
    {
        if (refreshing || busy)
            return;
        try
        {
            currentMap = (mapList.SelectedItem as ListBoxItem)?.Tag is string key ? host.ReadMap(key) : null;
        }
        catch (KeyNotFoundException)
        {
            currentMap = null;
        }
        evaluation = null;
        refreshing = true;
        playerSelector.Items.Clear();
        refreshing = false;
        populateLayers();
        region = fullRegion();
        onLayerChanged();
        showStep(false);
        attributes.Text = string.Empty;
        status.Text = localizer.Text(currentMap is null ? "mapUnavailable"
            : string.IsNullOrEmpty(savePath.Text) ? "ready" : "reloadForMap");
    }

    private void populateLayers(string? preferredLayer = null)
    {
        refreshing = true;
        layerSelector.Items.Clear();
        if (currentMap is not null)
            foreach (PluginMapLayerSnapshot layer in currentMap.Layers.Where(layer => layer.Visible))
                layerSelector.Items.Add(new ComboBoxItem { Content = layer.Name, Tag = layer.Name });
        layerSelector.SelectedItem = layerSelector.Items.OfType<ComboBoxItem>()
            .FirstOrDefault(item => string.Equals(item.Tag as string, preferredLayer, StringComparison.Ordinal))
            ?? layerSelector.Items.OfType<ComboBoxItem>().FirstOrDefault();
        refreshing = false;
    }

    private void onLayerChanged()
    {
        if (refreshing)
            return;
        string? name = (layerSelector.SelectedItem as ComboBoxItem)?.Tag as string;
        currentLayer = currentMap?.Layers.FirstOrDefault(layer => layer.Name == name);
        refreshMapPreview();
        updateControls();
    }

    private MapRegion fullRegion() => currentMap is null ? default : new(0, 0, currentMap.Width, currentMap.Height);

    private void setRegion(MapRegion value)
    {
        region = value;
        mapCanvas.SetRegion(region, !busy);
        regionText.Text = localizer.Format("region", value.X, value.Y, value.Width, value.Height);
        updateControls();
    }

    private void refreshMapPreview()
    {
        mapCanvas.SetMap(currentMap, currentLayer?.Name, Array.Empty<(int, int)>(), false);
        setRegion(region);
    }

    private async Task runAsync(Func<Task> action)
    {
        if (busy || closed)
            return;
        busy = true;
        updateControls();
        try
        {
            await action();
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
        }
        catch (Exception exception)
        {
            if (!closed)
                status.Text = localizer.Format("operationError", exception.Message);
        }
        finally
        {
            busy = false;
            if (closed)
                cancellation.Dispose();
            else
                updateControls();
        }
    }

    private void updateControls()
    {
        mapList.IsEnabled = !busy;
        layerSelector.IsEnabled = !busy;
        playerSelector.IsEnabled = !busy;
        browse.IsEnabled = !busy && currentMap is not null;
        reload.IsEnabled = !busy && currentMap is not null && !string.IsNullOrEmpty(savePath.Text);
        fullMap.IsEnabled = !busy && currentMap is not null;
        mapCanvas.IsEnabled = !busy;
        back.IsEnabled = !busy;
        categories.IsEnabled = !busy;
        bool ready = currentLayer is not null && evaluation is { Success: true }
            && region.Width > 0 && region.Height > 0;
        next.IsEnabled = !busy && ready;
        chooseBlueprints.IsEnabled = !busy && ready;
        generate.IsEnabled = !busy && ready && selected.Count > 0;
        progress.IsVisible = busy;
        counts.Text = localizer.Format("selectedCount", selected.Count);
    }
}
