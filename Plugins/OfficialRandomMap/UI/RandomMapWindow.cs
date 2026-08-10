using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
using Avalonia.Media;
using Ludork.Plugin.Abstractions;
using Ludork.Plugins.OfficialRandomMap.Generation;
using Ludork.Plugins.OfficialRandomMap.Localization;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Threading.Tasks;

namespace Ludork.Plugins.OfficialRandomMap.UI;

internal sealed class RandomMapWindow : Window
{
    private readonly IMapEditorHost host;
    private readonly PluginLocalizer localizer;
    private readonly CancellationTokenSource cancellation;
    private readonly ListBox mapList = new();
    private readonly ComboBox layerSelector = new();
    private readonly Button tileButton = new();
    private readonly TilePreviewControl tilePreview = new();
    private readonly TextBlock tileButtonLabel = new()
    {
        VerticalAlignment = VerticalAlignment.Center,
    };
    private readonly StackPanel tileButtonContent = new()
    {
        Orientation = Orientation.Horizontal,
        HorizontalAlignment = HorizontalAlignment.Center,
        Spacing = 10,
    };
    private readonly StackPanel stepPanel = new();
    private readonly TextBlock statusText = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
        TextWrapping = TextWrapping.Wrap,
    };
    private readonly TextBlock passableWarning = new()
    {
        Foreground = new SolidColorBrush(Color.Parse("#f9ab00")),
        TextWrapping = TextWrapping.Wrap,
        IsVisible = false,
    };
    private readonly RadioButton latticeButton = new();
    private readonly RadioButton roomsButton = new();
    private readonly Slider densitySlider = new()
    {
        Minimum = 35,
        Maximum = 75,
        TickFrequency = 1,
        IsSnapToTickEnabled = true,
        Value = 50,
        HorizontalAlignment = HorizontalAlignment.Stretch,
    };
    private readonly Grid densityHeader = new()
    {
        ColumnDefinitions = new ColumnDefinitions("*,Auto"),
    };
    private readonly TextBlock densityValueText = new();
    private readonly Button nextButton = new();
    private readonly Button backButton = new();
    private readonly Button clearMarkersButton = new();
    private readonly Button generateButton = new();
    private readonly Button closeButton = new();
    private readonly StackPanel tileActionPanel = new()
    {
        Orientation = Orientation.Horizontal,
        HorizontalAlignment = HorizontalAlignment.Right,
        Spacing = 8,
    };
    private readonly StackPanel markerActionPanel = new()
    {
        Orientation = Orientation.Horizontal,
        HorizontalAlignment = HorizontalAlignment.Right,
        Spacing = 8,
    };
    private readonly TextBlock markerCountText = new();
    private readonly RandomMapCanvas mapCanvas;
    private readonly HashSet<(int X, int Y)> markers = [];
    private PluginMapSnapshot? currentMap;
    private PluginMapLayerSnapshot? currentLayer;
    private PluginTilesetSnapshot? currentTileset;
    private int? selectedTile;
    private bool refreshingSelections;
    private bool markerStep;

    public RandomMapWindow(
        IMapEditorHost host,
        string suggestedMapKey,
        PluginLocalizer localizer,
        CancellationToken cancellationToken)
    {
        this.host = host;
        this.localizer = localizer;
        cancellation = CancellationTokenSource.CreateLinkedTokenSource(
            cancellationToken);
        mapCanvas = new RandomMapCanvas(host);

        Title = localizer.Text("windowTitle");
        Width = 1280;
        Height = 800;
        MinWidth = 980;
        MinHeight = 600;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        Background = new SolidColorBrush(Color.Parse("#161718"));

        configureControls();
        Content = createLayout();
        populateMaps(suggestedMapKey);
        Closed += (_, _) =>
        {
            cancellation.Cancel();
            cancellation.Dispose();
            tilePreview.Dispose();
            mapCanvas.Dispose();
        };
    }

    private void configureControls()
    {
        mapList.SelectionChanged += (_, _) => onMapSelectionChanged();
        layerSelector.HorizontalAlignment = HorizontalAlignment.Stretch;
        layerSelector.SelectionChanged += (_, _) => onLayerSelectionChanged();
        mapCanvas.MarkerToggled += onMarkerToggled;

        tileButton.HorizontalAlignment = HorizontalAlignment.Stretch;
        tileButton.HorizontalContentAlignment = HorizontalAlignment.Center;
        tileButton.Click += async (_, _) => await chooseTileAsync();
        tileButtonContent.Children.Add(tilePreview);
        tileButtonContent.Children.Add(tileButtonLabel);
        tileButton.Content = tileButtonContent;
        nextButton.Content = localizer.Text("next");
        nextButton.HorizontalAlignment = HorizontalAlignment.Right;
        nextButton.Click += (_, _) => showMarkerStep();
        backButton.Content = localizer.Text("back");
        backButton.Click += (_, _) => showTileStep();
        clearMarkersButton.Content = localizer.Text("clearMarkers");
        clearMarkersButton.Click += (_, _) =>
        {
            markers.Clear();
            updateMarkerState();
        };
        generateButton.Content = localizer.Text("generate");
        generateButton.Click += async (_, _) =>
        {
            try
            {
                await generateAsync();
            }
            catch (OperationCanceledException) when (
                cancellation.IsCancellationRequested)
            {
            }
            catch (Exception)
            {
                finishGeneration();
                statusText.Text = localizer.Text("unexpectedError");
            }
        };
        closeButton.Content = localizer.Text("close");
        closeButton.Click += (_, _) => Close();
        tileActionPanel.Children.Add(closeButton);
        tileActionPanel.Children.Add(nextButton);
        markerActionPanel.Children.Add(backButton);
        markerActionPanel.Children.Add(generateButton);
        latticeButton.Content = localizer.Text("lattice");
        latticeButton.GroupName = "RandomMapLayout";
        latticeButton.IsChecked = true;
        roomsButton.Content = localizer.Text("rooms");
        roomsButton.GroupName = "RandomMapLayout";
        densityHeader.Children.Add(
            createFieldLabel(localizer.Text("density")));
        Grid.SetColumn(densityValueText, 1);
        densityHeader.Children.Add(densityValueText);
        densitySlider.ValueChanged += (_, _) => updateDensityValue();
        updateDensityValue();
        passableWarning.Text = localizer.Text("passableWarning");
    }

    private Control createLayout()
    {
        TextBlock mapListTitle = createPanelTitle(localizer.Text("mapList"));
        Grid leftPanel = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,*"),
            RowSpacing = 8,
        };
        leftPanel.Children.Add(mapListTitle);
        Grid.SetRow(mapList, 1);
        leftPanel.Children.Add(mapList);

        TextBlock previewTitle = createPanelTitle(
            localizer.Text("mapPreview"));
        ScrollViewer mapScroll = new()
        {
            Content = mapCanvas,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        Grid previewPanel = new()
        {
            Margin = new Thickness(12),
            RowDefinitions = new RowDefinitions("Auto,*"),
            RowSpacing = 8,
        };
        previewPanel.Children.Add(previewTitle);
        Grid.SetRow(mapScroll, 1);
        previewPanel.Children.Add(mapScroll);

        Grid rightPanel = new()
        {
            Margin = new Thickness(14),
            RowDefinitions = new RowDefinitions("*,Auto"),
            RowSpacing = 12,
        };
        rightPanel.Children.Add(stepPanel);
        Border statusBorder = new()
        {
            Padding = new Thickness(12),
            Background = new SolidColorBrush(Color.Parse("#202124")),
            Child = statusText,
        };
        Grid.SetRow(statusBorder, 1);
        rightPanel.Children.Add(statusBorder);

        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("240,5,*,5,310"),
        };
        root.Children.Add(leftPanel);
        GridSplitter leftSplitter = createSplitter();
        Grid.SetColumn(leftSplitter, 1);
        root.Children.Add(leftSplitter);
        Grid.SetColumn(previewPanel, 2);
        root.Children.Add(previewPanel);
        GridSplitter rightSplitter = createSplitter();
        Grid.SetColumn(rightSplitter, 3);
        root.Children.Add(rightSplitter);
        Grid.SetColumn(rightPanel, 4);
        root.Children.Add(rightPanel);
        return root;
    }

    private void populateMaps(string suggestedMapKey)
    {
        refreshingSelections = true;
        mapList.Items.Clear();
        ListBoxItem? selected = null;
        foreach (PluginMapSummary map in host.ListMaps())
        {
            ListBoxItem item = new()
            {
                Content = new TextBlock
                {
                    Text = map.DisplayName,
                    TextTrimming = TextTrimming.CharacterEllipsis,
                },
                Tag = map,
            };
            ToolTip.SetTip(item, map.Key);
            mapList.Items.Add(item);
            if (string.Equals(
                    map.Key,
                    suggestedMapKey,
                    StringComparison.Ordinal))
            {
                selected = item;
            }
        }
        mapList.SelectedItem =
            selected ?? mapList.Items.OfType<ListBoxItem>().FirstOrDefault();
        refreshingSelections = false;
        onMapSelectionChanged();
    }

    private void onMapSelectionChanged()
    {
        if (refreshingSelections)
            return;
        if (mapList.SelectedItem is not ListBoxItem item
            || item.Tag is not PluginMapSummary map)
        {
            currentMap = null;
            resetWorkflow();
            statusText.Text = localizer.Text("chooseMap");
            return;
        }
        try
        {
            currentMap = host.ReadMap(map.Key);
        }
        catch (KeyNotFoundException)
        {
            currentMap = null;
            resetWorkflow();
            statusText.Text = localizer.Text("mapUnavailable");
            return;
        }
        resetWorkflow();
        populateLayers();
    }

    private void populateLayers()
    {
        refreshingSelections = true;
        layerSelector.Items.Clear();
        if (currentMap is not null)
        {
            foreach (PluginMapLayerSnapshot layer in currentMap.Layers)
            {
                ComboBoxItem item = new()
                {
                    Content = layer.Name,
                    Tag = layer,
                };
                layerSelector.Items.Add(item);
            }
        }
        layerSelector.SelectedItem =
            layerSelector.Items.OfType<ComboBoxItem>().FirstOrDefault();
        refreshingSelections = false;
        onLayerSelectionChanged();
    }

    private void onLayerSelectionChanged()
    {
        if (refreshingSelections)
            return;
        currentLayer =
            (layerSelector.SelectedItem as ComboBoxItem)?.Tag
            as PluginMapLayerSnapshot;
        selectedTile = null;
        currentTileset = null;
        if (currentLayer is not null
            && !string.IsNullOrWhiteSpace(currentLayer.TilesetKey))
        {
            try
            {
                currentTileset = host.ReadTileset(currentLayer.TilesetKey);
            }
            catch (KeyNotFoundException)
            {
                currentTileset = null;
            }
        }
        tilePreview.SetTile(currentTileset, null);
        updateTileButtonContent();
        passableWarning.IsVisible = false;
        nextButton.IsEnabled = false;
        mapCanvas.SetMap(
            currentMap,
            currentLayer?.Name,
            markers,
            markerStep);
        statusText.Text = currentLayer is null
            ? localizer.Text("chooseLayer")
            : currentTileset is null
                ? localizer.Text("missingTileset")
                : localizer.Text("ready");
    }

    private async Task chooseTileAsync()
    {
        if (currentTileset is null
            || currentTileset.TileCount <= 0
            || string.IsNullOrWhiteSpace(currentTileset.ImagePath)
            || !File.Exists(currentTileset.ImagePath))
        {
            statusText.Text = localizer.Text("missingTileset");
            return;
        }
        int? result;
        try
        {
            result = await TilesetSelectionDialog.ShowAsync(
                this,
                currentTileset,
                selectedTile,
                localizer);
        }
        catch (Exception)
        {
            statusText.Text = localizer.Text("missingTileset");
            return;
        }
        if (result is not int tile)
            return;
        selectedTile = tile;
        tilePreview.SetTile(currentTileset, selectedTile);
        updateTileButtonContent();
        passableWarning.IsVisible =
            tile >= 0
            && tile < currentTileset.Passable.Count
            && currentTileset.Passable[tile];
        nextButton.IsEnabled = true;
        statusText.Text = localizer.Text("ready");
    }

    private void showTileStep()
    {
        markerStep = false;
        mapCanvas.SetMarkers(markers, false);
        rebuildTileStep();
    }

    private void showMarkerStep()
    {
        if (selectedTile is null)
        {
            statusText.Text = localizer.Text("chooseTileFirst");
            return;
        }
        markerStep = true;
        mapCanvas.SetMarkers(markers, true);
        rebuildMarkerStep();
    }

    private void rebuildTileStep()
    {
        stepPanel.Children.Clear();
        stepPanel.Spacing = 12;
        stepPanel.Children.Add(createFieldLabel(localizer.Text("selectLayer")));
        stepPanel.Children.Add(layerSelector);
        stepPanel.Children.Add(createFieldLabel(localizer.Text("selectTile")));
        stepPanel.Children.Add(tileButton);
        stepPanel.Children.Add(passableWarning);
        stepPanel.Children.Add(tileActionPanel);
    }

    private void rebuildMarkerStep()
    {
        stepPanel.Children.Clear();
        stepPanel.Spacing = 12;
        TextBlock hint = new()
        {
            Text = localizer.Text("markerHint"),
            Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
            TextWrapping = TextWrapping.Wrap,
        };
        stepPanel.Children.Add(hint);
        stepPanel.Children.Add(markerCountText);
        stepPanel.Children.Add(clearMarkersButton);
        stepPanel.Children.Add(createFieldLabel(localizer.Text("layout")));
        stepPanel.Children.Add(latticeButton);
        stepPanel.Children.Add(roomsButton);
        stepPanel.Children.Add(densityHeader);
        stepPanel.Children.Add(densitySlider);
        TextBlock densityHint = new()
        {
            Text = localizer.Text("densityHint"),
            Foreground = new SolidColorBrush(Color.Parse("#bdbdbd")),
            TextWrapping = TextWrapping.Wrap,
        };
        stepPanel.Children.Add(densityHint);
        stepPanel.Children.Add(markerActionPanel);
        updateMarkerState();
    }

    private async Task generateAsync()
    {
        if (currentMap is null
            || currentLayer is null
            || currentTileset is null
            || selectedTile is not int wallTile)
        {
            statusText.Text = localizer.Text("chooseTileFirst");
            return;
        }
        if (wallTile < 0 || wallTile >= currentTileset.TileCount)
        {
            statusText.Text = localizer.Text("invalidTile");
            return;
        }

        generateButton.IsEnabled = false;
        backButton.IsEnabled = false;
        mapList.IsEnabled = false;
        mapCanvas.IsEnabled = false;
        clearMarkersButton.IsEnabled = false;
        latticeButton.IsEnabled = false;
        roomsButton.IsEnabled = false;
        densitySlider.IsEnabled = false;
        statusText.Text = localizer.Text("generating");
        int seed = RandomNumberGenerator.GetInt32(int.MaxValue);
        int densityPercent = (int)densitySlider.Value;
        RandomMapMode mode = roomsButton.IsChecked == true
            ? RandomMapMode.Rooms
            : RandomMapMode.Lattice;
        RandomMapPoint[] points = markers
            .Select(marker => new RandomMapPoint(marker.X, marker.Y))
            .ToArray();
        RandomMapGenerationResult generated = await Task.Run(
            () => RandomMapGenerator.Generate(
                currentMap.Width,
                currentMap.Height,
                wallTile,
                points,
                mode,
                densityPercent,
                seed,
                cancellation.Token),
            cancellation.Token);
        if (!generated.Success)
        {
            finishGeneration();
            statusText.Text = string.IsNullOrWhiteSpace(generated.Error)
                ? localizer.Text("noValidLayout")
                : localizer.Text(generated.Error);
            return;
        }

        List<IReadOnlyList<string?>> autoTiles = new(currentMap.Height);
        for (int y = 0; y < currentMap.Height; y++)
        {
            string?[] row = new string?[currentMap.Width];
            autoTiles.Add(row);
        }
        PluginMapLayerWriteRequest request = new(
            currentMap.Key,
            currentLayer.Name,
            currentMap.Revision,
            currentLayer.TilesetKey,
            generated.Tiles,
            autoTiles);
        PluginMapWriteResult result = await host.ReplaceLayerAndSaveAsync(
            request,
            cancellation.Token);
        if (result.Conflict)
        {
            string mapKey = currentMap.Key;
            currentMap = host.ReadMap(mapKey);
            resetWorkflow();
            populateLayers();
            statusText.Text = localizer.Text("conflict");
            return;
        }
        if (!result.Success)
        {
            finishGeneration();
            statusText.Text = localizer.Text("saveFailed");
            return;
        }

        int generatedCellCount = generated.Tiles.Sum(row => row.Count);
        int generatedWallCount = generated.Tiles.Sum(
            row => row.Count(tile => tile is not null));
        double actualDensity = generatedCellCount == 0
            ? 0
            : generatedWallCount * 100.0 / generatedCellCount;
        string layerName = currentLayer.Name;
        int retainedTile = wallTile;
        currentMap = host.ReadMap(currentMap.Key);
        currentLayer = currentMap.Layers.First(layer =>
            string.Equals(layer.Name, layerName, StringComparison.Ordinal));
        currentTileset = host.ReadTileset(currentLayer.TilesetKey);
        selectedTile = retainedTile;
        mapCanvas.SetMap(
            currentMap,
            currentLayer.Name,
            markers,
            true);
        generateButton.Content = localizer.Text("regenerate");
        statusText.Text = localizer.Format(
            "saved",
            generated.Seed,
            actualDensity);
        finishGeneration();
    }

    private void finishGeneration()
    {
        generateButton.IsEnabled = true;
        backButton.IsEnabled = true;
        mapList.IsEnabled = true;
        mapCanvas.IsEnabled = true;
        clearMarkersButton.IsEnabled = markers.Count != 0;
        latticeButton.IsEnabled = true;
        roomsButton.IsEnabled = true;
        densitySlider.IsEnabled = true;
    }

    private void onMarkerToggled(
        object? sender,
        MapMarkerEventArgs args)
    {
        (int X, int Y) marker = (args.X, args.Y);
        if (!markers.Add(marker))
            markers.Remove(marker);
        updateMarkerState();
    }

    private void updateMarkerState()
    {
        markerCountText.Text = localizer.Format(
            "markers",
            markers.Count);
        clearMarkersButton.IsEnabled = markers.Count != 0;
        mapCanvas.SetMarkers(markers, markerStep);
    }

    private void resetWorkflow()
    {
        markers.Clear();
        selectedTile = null;
        currentLayer = null;
        currentTileset = null;
        markerStep = false;
        generateButton.Content = localizer.Text("generate");
        generateButton.IsEnabled = true;
        backButton.IsEnabled = true;
        mapList.IsEnabled = true;
        mapCanvas.IsEnabled = true;
        latticeButton.IsEnabled = true;
        roomsButton.IsEnabled = true;
        densitySlider.IsEnabled = true;
        tilePreview.SetTile(null, null);
        rebuildTileStep();
        mapCanvas.SetMap(currentMap, null, markers, false);
    }

    private void updateTileButtonContent()
    {
        tileButtonLabel.Text = selectedTile is null
            ? localizer.Text("selectTile")
            : localizer.Text("changeTile");
    }

    private void updateDensityValue()
    {
        densityValueText.Text = localizer.Format(
            "densityValue",
            (int)densitySlider.Value);
    }

    private static TextBlock createPanelTitle(string text)
    {
        return new TextBlock
        {
            Text = text,
            FontSize = 16,
            FontWeight = FontWeight.SemiBold,
        };
    }

    private static TextBlock createFieldLabel(string text)
    {
        return new TextBlock
        {
            Text = text,
            FontWeight = FontWeight.SemiBold,
        };
    }

    private static GridSplitter createSplitter()
    {
        return new GridSplitter
        {
            Width = 5,
            ResizeDirection = GridResizeDirection.Columns,
            Background = new SolidColorBrush(Color.Parse("#353535")),
        };
    }
}
