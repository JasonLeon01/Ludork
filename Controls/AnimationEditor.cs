using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Plugin.Avalonia;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;
using InputKey = Avalonia.Input.Key;

namespace Ludork.Controls;

public sealed class AnimationEditor : UserControl
{
    private const double AssetMarqueeThreshold = 4;
    private readonly GameDataService gameData;
    private readonly string key;
    private readonly TextBox nameBox = EditorInputs.CreateEditableTextBox();
    private readonly ComboBox fpsBox = new();
    private readonly Grid assetGrid = new();
    private readonly Grid assetSelectionSurface = new();
    private readonly Border assetSelectionBox = new();
    private readonly HashSet<int> assetSelectionBeforeMarquee = [];
    private readonly HashSet<int> selectedAssetIndexes = [];
    private int assetSelectionAnchor = -1;
    private Point assetMarqueeStart;
    private bool assetMarqueePending;
    private bool assetMarqueeActive;
    private bool assetMarqueeAdditive;
    private bool assetMarqueeToggle;
    private readonly CheckBox flipX = new() { Content = "Flip X" };
    private readonly TextBox startTime = EditorInputs.CreateEditableTextBox();
    private readonly TextBox startX = EditorInputs.CreateEditableTextBox();
    private readonly TextBox startY = EditorInputs.CreateEditableTextBox();
    private readonly TextBox startRotation = EditorInputs.CreateEditableTextBox();
    private readonly TextBox startScaleX = EditorInputs.CreateEditableTextBox();
    private readonly TextBox startScaleY = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endTime = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endX = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endY = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endRotation = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endScaleX = EditorInputs.CreateEditableTextBox();
    private readonly TextBox endScaleY = EditorInputs.CreateEditableTextBox();
    private readonly AnimationPreview preview;
    private readonly AnimationTimeline timeline;
    private readonly Slider zoomSlider = new()
    {
        Minimum = 20,
        Maximum = 500,
        Value = 100,
        Width = 120,
    };
    private readonly DispatcherTimer playbackTimer = new();
    private readonly Stopwatch playbackClock = new();
    private readonly Dictionary<(int Track, int Segment), IAnimationAudioPlayback> soundPlayers = [];
    private Button? playbackButton;
    private JsonObject data;
    private int selectedTrack = -1;
    private int selectedSegment = -1;
    private bool loadingInspector;
    private bool isPlaying;
    private bool updatingZoomSlider;
    private static (int Track, JsonObject Segment)? segmentClipboard;

    public AnimationEditor(GameDataService gameData, string key, JsonObject data)
    {
        this.gameData = gameData;
        this.key = key;
        this.data = (JsonObject)data.DeepClone();

        preview = new AnimationPreview(gameData.ProjectPath, () => this.data);
        timeline = new AnimationTimeline(gameData.ProjectPath, () => this.data);
        preview.SegmentSelected += selectSingleSegment;
        preview.SegmentChanged += onSegmentChanged;
        timeline.SegmentSelected += selectSegment;
        timeline.SegmentChanged += onSegmentChanged;
        timeline.TimeTagChanged += commit;
        timeline.TimeTagRenameRequested += index => _ = renameTimeTag(index);
        timeline.ZoomChanged += onTimelineZoomChanged;
        timeline.TimeChanged += time =>
        {
            preview.CurrentTime = time;
            preview.Refresh();
        };
        playbackTimer.Interval = TimeSpan.FromMilliseconds(1000.0 / 60.0);
        playbackTimer.Tick += (_, _) => advancePlayback();
        AddHandler(KeyDownEvent, onEditorKeyDown, RoutingStrategies.Tunnel);

        buildLayout();
        refreshEditor();
    }

    public event EventHandler? Modified;

    public string Key => key;

    public void Reload(JsonObject nextData)
    {
        data = (JsonObject)nextData.DeepClone();
        selectedTrack = -1;
        selectedSegment = -1;
        timeline.SetSegmentSelection(-1, -1);
        timeline.ClearTimeTagSelection();
        preview.SelectedTrack = -1;
        preview.SelectedSegment = -1;
        selectedAssetIndexes.Clear();
        assetSelectionAnchor = -1;
        refreshEditor();
    }

    private void buildLayout()
    {
        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("350,*"),
            ColumnSpacing = 8,
            Margin = new Thickness(8),
        };

        ScrollViewer leftScroll = new() { VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
        StackPanel left = new() { Spacing = 6 };
        leftScroll.Content = left;
        root.Children.Add(leftScroll);

        left.Children.Add(new TextBlock { Text = LocaleService.Get("ANIMATION_NAME") });
        nameBox.TextChanged += (_, _) =>
        {
            if (!loadingInspector)
            {
                data["name"] = nameBox.Text ?? string.Empty;
                commit();
            }
        };
        left.Children.Add(nameBox);

        left.Children.Add(new TextBlock { Text = LocaleService.Get("FRAME_RATE") });
        fpsBox.ItemsSource = new[] { "30", "60" };
        fpsBox.SelectionChanged += (_, _) =>
        {
            if (loadingInspector || fpsBox.SelectedItem is not string value || !int.TryParse(value, out int frameRate))
                return;
            data["frameRate"] = frameRate;
            timeline.Refresh();
            commit();
        };
        left.Children.Add(fpsBox);

        left.Children.Add(new TextBlock { Text = LocaleService.Get("ASSETS") });

        assetGrid.HorizontalAlignment = HorizontalAlignment.Left;
        assetGrid.VerticalAlignment = VerticalAlignment.Top;
        assetGrid.ColumnSpacing = 5;
        assetGrid.RowSpacing = 5;
        assetGrid.Margin = new Thickness(9);
        for (int column = 0; column < 3; column += 1)
            assetGrid.ColumnDefinitions.Add(new ColumnDefinition(new GridLength(64)));
        ScrollViewer assetsScroll = new()
        {
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            Content = assetGrid,
        };
        assetSelectionBox.Background = new SolidColorBrush(Color.Parse("#338ab4f8"));
        assetSelectionBox.BorderBrush = new SolidColorBrush(Color.Parse("#8ab4f8"));
        assetSelectionBox.BorderThickness = new Thickness(1);
        assetSelectionBox.CornerRadius = new CornerRadius(2);
        assetSelectionBox.IsHitTestVisible = false;
        assetSelectionBox.IsVisible = false;
        Canvas assetSelectionOverlay = new() { IsHitTestVisible = false };
        assetSelectionOverlay.Children.Add(assetSelectionBox);
        assetSelectionSurface.Height = 256;
        assetSelectionSurface.Background = Brushes.Transparent;
        assetSelectionSurface.ClipToBounds = true;
        assetSelectionSurface.Children.Add(assetsScroll);
        assetSelectionSurface.Children.Add(assetSelectionOverlay);
        assetSelectionSurface.PointerPressed += onAssetsPointerPressed;
        assetSelectionSurface.PointerMoved += onAssetsPointerMoved;
        assetSelectionSurface.PointerReleased += onAssetsPointerReleased;
        assetSelectionSurface.AddHandler(
            PointerCaptureLostEvent,
            onAssetsPointerCaptureLost,
            RoutingStrategies.Tunnel
        );
        left.Children.Add(assetSelectionSurface);

        left.Children.Add(new Separator());
        left.Children.Add(new TextBlock { Text = LocaleService.Get("SEGMENT_PROPERTIES"), FontWeight = FontWeight.Bold });
        left.Children.Add(flipX);
        flipX.IsEnabled = false;
        flipX.IsCheckedChanged += (_, _) => updateInspector();
        addFrameRows(left, LocaleService.Get("startFrame"), startTime, startX, startY, startRotation, startScaleX, startScaleY);
        addFrameRows(left, LocaleService.Get("endFrame"), endTime, endX, endY, endRotation, endScaleX, endScaleY);

        Grid right = new() { RowDefinitions = new RowDefinitions("*,260"), RowSpacing = 8 };
        Grid.SetColumn(right, 1);
        right.Children.Add(preview);
        Grid timelineArea = new() { RowDefinitions = new RowDefinitions("28,*"), RowSpacing = 0 };
        Border toolbar = new() { Background = new SolidColorBrush(Color.Parse("#333333")), BorderBrush = new SolidColorBrush(Color.Parse("#222222")), BorderThickness = new Thickness(0, 0, 0, 1) };
        StackPanel transport = new() { Orientation = Orientation.Horizontal, Spacing = 8, Margin = new Thickness(8, 2) };
        playbackButton = new Button
        {
            Content = LocaleService.Get("PLAY_ANIMATION"),
            MinWidth = 92,
            Height = 24,
            Padding = new Thickness(12, 1),
        };
        playbackButton.Click += (_, _) => togglePlayback();
        transport.Children.Add(playbackButton);
        Button addTimeTag = new()
        {
            Content = LocaleService.Get("ADD_TIME_TAG"),
            Height = 24,
            Padding = new Thickness(12, 1),
        };
        addTimeTag.Click += async (_, _) => await addTimeTagAtPlayhead();
        transport.Children.Add(addTimeTag);
        transport.Children.Add(new TextBlock { Text = "Zoom", Foreground = new SolidColorBrush(Color.Parse("#aaaaaa")), VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(12, 0, 0, 0) });
        zoomSlider.PropertyChanged += (_, args) =>
        {
            if (args.Property == RangeBase.ValueProperty && !updatingZoomSlider)
                timeline.SetZoom(zoomSlider.Value / 100.0);
        };
        transport.Children.Add(zoomSlider);
        toolbar.Child = transport;
        timelineArea.Children.Add(toolbar);
        ScrollViewer timelineScroll = new()
        {
            HorizontalScrollBarVisibility = ScrollBarVisibility.Visible,
            VerticalScrollBarVisibility = ScrollBarVisibility.Visible,
            Content = timeline,
        };
        Grid.SetRow(timelineScroll, 1);
        timelineArea.Children.Add(timelineScroll);
        Grid.SetRow(timelineArea, 1);
        right.Children.Add(timelineArea);
        root.Children.Add(right);
        Content = root;
    }

    private void onTimelineZoomChanged(double value)
    {
        double sliderValue = value * 100.0;
        if (Math.Abs(zoomSlider.Value - sliderValue) < 0.0001)
            return;
        updatingZoomSlider = true;
        zoomSlider.Value = sliderValue;
        updatingZoomSlider = false;
    }

    private void addFrameRows(StackPanel target, string title, params TextBox[] fields)
    {
        target.Children.Add(new TextBlock { Text = title, FontWeight = FontWeight.SemiBold, Margin = new Thickness(0, 6, 0, 0) });
        string[] labels = ["Time", "X", "Y", "Rotation", "Scale X", "Scale Y"];
        for (int index = 0; index < fields.Length; index += 1)
        {
            TextBox field = fields[index];
            field.IsEnabled = false;
            field.TextChanged += (_, _) => updateInspector();
            Grid row = new() { ColumnDefinitions = new ColumnDefinitions("88,*"), ColumnSpacing = 6 };
            row.Children.Add(new TextBlock { Text = labels[index], VerticalAlignment = VerticalAlignment.Center });
            Grid.SetColumn(field, 1);
            row.Children.Add(field);
            target.Children.Add(row);
        }
    }

    private async System.Threading.Tasks.Task addAssets(bool audio)
    {
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        string root = Path.Combine(gameData.ProjectPath, "Assets", audio ? "Sounds" : "Animations");
        Directory.CreateDirectory(root);
        string? path = await FileSelectorDialog.ShowAsync(owner, root,
            FileSelectorDialog.AllFilesFilter(star: true), LocaleService.Get(audio ? "ADD_AUDIO" : "ADD_ASSET"));
        if (path is null)
            return;
        JsonArray assets = getAssets();
        assets.Add(Path.GetFileName(path));
        refreshAssets();
        commit();
    }

    private async System.Threading.Tasks.Task addTimeTagAtPlayhead()
    {
        if (TopLevel.GetTopLevel(this) is not Window owner)
            return;
        string? tag = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("ADD_TIME_TAG"),
            LocaleService.Get("TIME_TAG_NAME"),
            Array.Empty<string>()
        );
        if (tag is not null)
            timeline.AddTimeTag(tag);
    }

    private async System.Threading.Tasks.Task renameTimeTag(int index)
    {
        if (TopLevel.GetTopLevel(this) is not Window owner || timeline.GetTimeTag(index) is not JsonObject timeTag)
            return;
        string current = timeTag["tag"]?.GetValue<string>() ?? string.Empty;
        string? tag = await SingleRowDialog.ShowAsync(
            owner,
            LocaleService.Get("RENAME_TIME_TAG"),
            LocaleService.Get("TIME_TAG_NAME"),
            Array.Empty<string>(),
            current
        );
        if (tag is not null)
            timeline.RenameTimeTag(index, tag);
    }

    private void onAssetsPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        if (isAssetScrollBarSource(args.Source))
            return;
        PointerPoint point = args.GetCurrentPoint(assetSelectionSurface);
        Point position = args.GetPosition(assetSelectionSurface);
        int assetIndex = getAssetIndexFromSource(args.Source);
        if (assetIndex < 0)
            assetIndex = getAssetIndexAtPosition(position);
        if (point.Properties.IsRightButtonPressed)
        {
            showAssetsContextMenu(assetIndex);
            args.Handled = true;
            return;
        }
        if (args.Pointer.Type != PointerType.Mouse
            || !point.Properties.IsLeftButtonPressed
            || assetIndex >= 0)
        {
            return;
        }
        assetMarqueeStart = position;
        assetMarqueePending = true;
        assetMarqueeActive = false;
        assetMarqueeAdditive = args.KeyModifiers.HasFlag(KeyModifiers.Shift);
        assetMarqueeToggle = !assetMarqueeAdditive
            && EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        assetSelectionBeforeMarquee.Clear();
        assetSelectionBeforeMarquee.UnionWith(selectedAssetIndexes);
        assetSelectionBox.IsVisible = false;
        args.Pointer.Capture(assetSelectionSurface);
        args.Handled = true;
    }

    private void onAssetsPointerMoved(object? sender, PointerEventArgs args)
    {
        if (!assetMarqueePending)
            return;
        PointerPoint point = args.GetCurrentPoint(assetSelectionSurface);
        if (args.Pointer.Type != PointerType.Mouse
            || !point.Properties.IsLeftButtonPressed)
        {
            completeAssetMarquee(true);
            args.Pointer.Capture(null);
            return;
        }
        Point position = args.GetPosition(assetSelectionSurface);
        if (!assetMarqueeActive
            && Math.Abs(position.X - assetMarqueeStart.X) < AssetMarqueeThreshold
            && Math.Abs(position.Y - assetMarqueeStart.Y) < AssetMarqueeThreshold)
        {
            return;
        }
        assetMarqueeActive = true;
        updateAssetMarquee(position);
        args.Handled = true;
    }

    private void onAssetsPointerReleased(object? sender, PointerReleasedEventArgs args)
    {
        if (!assetMarqueePending)
            return;
        if (assetMarqueeActive)
            updateAssetMarquee(args.GetPosition(assetSelectionSurface));
        completeAssetMarquee(true);
        args.Pointer.Capture(null);
        args.Handled = true;
    }

    private void onAssetsPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        completeAssetMarquee(false);
    }

    private void updateAssetMarquee(Point position)
    {
        Rect rectangle = createMarqueeRect(
            assetMarqueeStart,
            position,
            new Rect(assetSelectionSurface.Bounds.Size)
        );
        Canvas.SetLeft(assetSelectionBox, rectangle.X);
        Canvas.SetTop(assetSelectionBox, rectangle.Y);
        assetSelectionBox.Width = rectangle.Width;
        assetSelectionBox.Height = rectangle.Height;
        assetSelectionBox.IsVisible = true;

        List<int> hitIndexes = [];
        foreach (Control child in assetGrid.Children)
        {
            if (child is not Border item || item.Tag is not int index)
                continue;
            Point? origin = item.TranslatePoint(new Point(), assetSelectionSurface);
            if (origin is null)
                continue;
            Rect itemBounds = new(origin.Value, item.Bounds.Size);
            if (rectsOverlap(rectangle, itemBounds))
                hitIndexes.Add(index);
        }
        HashSet<int> nextSelection = buildMarqueeSelection(
            assetSelectionBeforeMarquee,
            hitIndexes,
            assetMarqueeAdditive,
            assetMarqueeToggle
        );
        selectedAssetIndexes.Clear();
        selectedAssetIndexes.UnionWith(nextSelection);
        refreshAssetSelection();
    }

    private void completeAssetMarquee(bool clearOnBlank)
    {
        if (!assetMarqueePending)
            return;
        if (!assetMarqueeActive)
        {
            if (clearOnBlank && !assetMarqueeAdditive && !assetMarqueeToggle)
            {
                selectedAssetIndexes.Clear();
                assetSelectionAnchor = -1;
                refreshAssetSelection();
            }
        }
        else if ((!assetMarqueeAdditive && !assetMarqueeToggle)
            || assetSelectionAnchor < 0)
        {
            assetSelectionAnchor = selectedAssetIndexes.Count == 0
                ? -1
                : selectedAssetIndexes.Min();
        }
        assetMarqueePending = false;
        assetMarqueeActive = false;
        assetMarqueeAdditive = false;
        assetMarqueeToggle = false;
        assetSelectionBeforeMarquee.Clear();
        assetSelectionBox.IsVisible = false;
    }

    private int getAssetIndexAtPosition(Point position)
    {
        foreach (Control child in assetGrid.Children)
        {
            if (child is not Border item || item.Tag is not int index)
                continue;
            Point? origin = item.TranslatePoint(new Point(), assetSelectionSurface);
            if (origin is not null
                && new Rect(origin.Value, item.Bounds.Size).Contains(position))
            {
                return index;
            }
        }
        return -1;
    }

    private static int getAssetIndexFromSource(object? source)
    {
        if (source is Border sourceBorder && sourceBorder.Tag is int sourceIndex)
            return sourceIndex;
        Border? item = (source as Visual)?.GetVisualAncestors()
            .OfType<Border>()
            .FirstOrDefault(border => border.Tag is int);
        return item?.Tag is int index ? index : -1;
    }

    private static bool isAssetScrollBarSource(object? source)
    {
        if (source is ScrollBar)
            return true;
        return (source as Visual)?.GetVisualAncestors()
            .OfType<ScrollBar>()
            .Any() == true;
    }

    internal static Rect createMarqueeRect(
        Point start,
        Point finish,
        Rect bounds
    )
    {
        double left = Math.Clamp(Math.Min(start.X, finish.X), bounds.Left, bounds.Right);
        double top = Math.Clamp(Math.Min(start.Y, finish.Y), bounds.Top, bounds.Bottom);
        double right = Math.Clamp(Math.Max(start.X, finish.X), bounds.Left, bounds.Right);
        double bottom = Math.Clamp(Math.Max(start.Y, finish.Y), bounds.Top, bounds.Bottom);
        return new Rect(left, top, right - left, bottom - top);
    }

    internal static bool rectsOverlap(Rect first, Rect second)
    {
        return first.Left <= second.Right
            && first.Right >= second.Left
            && first.Top <= second.Bottom
            && first.Bottom >= second.Top;
    }

    internal static HashSet<T> buildMarqueeSelection<T>(
        IEnumerable<T> initialSelection,
        IEnumerable<T> hitItems,
        bool additive,
        bool toggle
    ) where T : notnull
    {
        HashSet<T> selection = additive || toggle
            ? [.. initialSelection]
            : [];
        foreach (T item in hitItems)
        {
            if (toggle && !selection.Add(item))
                selection.Remove(item);
            else if (!toggle)
                selection.Add(item);
        }
        return selection;
    }

    private void showAssetsContextMenu(int assetIndex)
    {
        MenuItem addImage = new() { Header = LocaleService.Get("ADD_ASSET") };
        addImage.Click += async (_, _) => await addAssets(false);
        MenuItem addAudio = new() { Header = LocaleService.Get("ADD_AUDIO") };
        addAudio.Click += async (_, _) => await addAssets(true);
        List<object> items = [addImage, addAudio];
        if (assetIndex >= 0)
        {
            MenuItem delete = new() { Header = LocaleService.Get("DELETE") };
            delete.Click += (_, _) =>
            {
                selectedAssetIndexes.Clear();
                selectedAssetIndexes.Add(assetIndex);
                assetSelectionAnchor = assetIndex;
                removeSelectedAsset();
            };
            items.Add(delete);
        }
        ContextMenu menu = new() { ItemsSource = items };
        assetSelectionSurface.ContextMenu = menu;
        menu.Open(assetSelectionSurface);
    }

    private void addSelectedAssetSegment()
    {
        if (selectedAssetIndexes.Count == 0)
            return;
        int assetIndex = selectedAssetIndexes.Min();
        string assetName = getAssets()[assetIndex]?.GetValue<string>() ?? string.Empty;
        bool audio = isAudioAsset(assetName);
        double start = timeline.CurrentTime;
        double duration = audio ? 1.0 : 0.05;
        JsonObject segment = new()
        {
            ["type"] = audio ? "sound" : "frame",
            ["asset"] = assetIndex,
            ["flipX"] = false,
            ["startFrame"] = createFrame(start),
            ["endFrame"] = createFrame(start + duration),
        };
        if (audio)
            segment["originalDuration"] = duration;
        int track = timeline.FindAvailableTrack(start, start + duration);
        JsonArray lines = getTimeLines();
        while (lines.Count <= track)
            lines.Add(new JsonObject { ["timeSegments"] = new JsonArray() });
        ((JsonObject)lines[track]!)["timeSegments"]!.AsArray().Add(segment);
        selectSingleSegment(track, ((JsonObject)lines[track]!)["timeSegments"]!.AsArray().Count - 1);
        commit();
    }

    private void removeSelectedAsset()
    {
        if (selectedAssetIndexes.Count == 0)
            return;
        int assetIndex = selectedAssetIndexes.Min();
        JsonArray assets = getAssets();
        if (assetIndex < 0 || assetIndex >= assets.Count)
            return;
        assets.RemoveAt(assetIndex);
        foreach (JsonNode? lineNode in getTimeLines())
        {
            if (lineNode is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int index = segments.Count - 1; index >= 0; index -= 1)
            {
                if (segments[index] is not JsonObject segment)
                    continue;
                int segmentAsset = (int)number(segment["asset"], -1);
                if (segmentAsset == assetIndex)
                    segments.RemoveAt(index);
                else if (segmentAsset > assetIndex)
                    segment["asset"] = segmentAsset - 1;
            }
        }
        selectedAssetIndexes.Clear();
        assetSelectionAnchor = -1;
        refreshAssets();
        clearSelection();
        commit();
    }

    private void deleteSelectedSegment()
    {
        JsonArray lines = getTimeLines();
        if (selectedTrack < 0 || selectedTrack >= lines.Count || lines[selectedTrack] is not JsonObject line || line["timeSegments"] is not JsonArray segments
            || selectedSegment < 0 || selectedSegment >= segments.Count)
            return;
        segments.RemoveAt(selectedSegment);
        clearSelection();
        commit();
    }

    private void selectSegment(int track, int segment)
    {
        if (track >= 0 && segment >= 0)
            timeline.ClearTimeTagSelection();
        selectedTrack = track;
        selectedSegment = segment;
        timeline.SelectedTrack = track;
        timeline.SelectedSegment = segment;
        preview.SelectedTrack = track;
        preview.SelectedSegment = segment;
        timeline.Refresh();
        preview.Refresh();
        loadInspector();
    }

    private void selectSingleSegment(int track, int segment)
    {
        timeline.SetSegmentSelection(track, segment);
        selectSegment(track, segment);
    }

    private void clearSelection()
    {
        selectedTrack = -1;
        selectedSegment = -1;
        timeline.SetSegmentSelection(-1, -1);
        timeline.ClearTimeTagSelection();
        preview.SelectedTrack = -1;
        preview.SelectedSegment = -1;
        timeline.Refresh();
        preview.Refresh();
        loadInspector();
    }

    private void loadInspector()
    {
        loadingInspector = true;
        JsonObject? segment = getSelectedSegment();
        bool valid = segment is not null;
        foreach (TextBox box in inspectorFields())
            box.IsEnabled = valid;
        flipX.IsEnabled = valid && segment?["type"]?.GetValue<string>() == "frame";
        if (!valid)
        {
            foreach (TextBox box in inspectorFields())
                box.Text = string.Empty;
            flipX.IsChecked = false;
            loadingInspector = false;
            return;
        }
        JsonObject start = ensureFrame(segment!, "startFrame");
        JsonObject end = ensureFrame(segment!, "endFrame");
        setFrameFields(start, startTime, startX, startY, startRotation, startScaleX, startScaleY);
        setFrameFields(end, endTime, endX, endY, endRotation, endScaleX, endScaleY);
        flipX.IsChecked = segment!["flipX"]?.GetValue<bool>() ?? false;
        loadingInspector = false;
    }

    private void updateInspector()
    {
        if (loadingInspector || getSelectedSegment() is not JsonObject segment)
            return;
        if (!tryReadFrame(startTime, startX, startY, startRotation, startScaleX, startScaleY, out JsonObject start)
            || !tryReadFrame(endTime, endX, endY, endRotation, endScaleX, endScaleY, out JsonObject end))
            return;
        if (number(end["time"]) < number(start["time"]) + (segment["type"]?.GetValue<string>() == "sound" ? 1.0 / frameRate() : 0.05))
            return;
        segment["startFrame"] = start;
        segment["endFrame"] = end;
        segment["flipX"] = flipX.IsChecked == true;
        onSegmentChanged();
    }

    private void onSegmentChanged()
    {
        loadInspector();
        commit();
    }

    private void commit()
    {
        gameData.UpdateAnimation(key, data);
        preview.Refresh();
        timeline.Refresh();
        Modified?.Invoke(this, EventArgs.Empty);
    }

    private void togglePlayback()
    {
        if (isPlaying)
        {
            stopPlayback();
            return;
        }
        double duration = maxTime();
        if (duration <= 0)
        {
            timeline.SetTime(0);
            return;
        }
        isPlaying = true;
        timeline.SetTime(0, false);
        playbackClock.Restart();
        playbackTimer.Start();
        playbackButton!.Content = LocaleService.Get("STOP_ANIMATION");
    }

    private void advancePlayback()
    {
        double time = playbackClock.Elapsed.TotalSeconds;
        double duration = maxTime();
        if (time >= duration)
        {
            timeline.SetTime(duration, false);
            stopPlayback();
            return;
        }
        timeline.SetTime(time, false);
        syncPlaybackSounds(time);
    }

    private void stopPlayback()
    {
        isPlaying = false;
        playbackTimer.Stop();
        playbackClock.Reset();
        foreach (IAnimationAudioPlayback player in soundPlayers.Values)
            player.Dispose();
        soundPlayers.Clear();
        if (playbackButton is not null)
            playbackButton.Content = LocaleService.Get("PLAY_ANIMATION");
    }

    private void syncPlaybackSounds(double time)
    {
        HashSet<(int Track, int Segment)> active = [];
        JsonArray assets = getAssets();
        JsonArray lines = getTimeLines();
        for (int track = 0; track < lines.Count; track += 1)
        {
            if (lines[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int segmentIndex = 0; segmentIndex < segments.Count; segmentIndex += 1)
            {
                if (segments[segmentIndex] is not JsonObject segment || segment["type"]?.GetValue<string>() != "sound")
                    continue;
                double start = number((segment["startFrame"] as JsonObject)?["time"]);
                double end = number((segment["endFrame"] as JsonObject)?["time"]);
                if (time < start || time >= end)
                    continue;
                (int Track, int Segment) playerKey = (track, segmentIndex);
                active.Add(playerKey);
                if (soundPlayers.ContainsKey(playerKey))
                    continue;
                int assetIndex = (int)number(segment["asset"], -1);
                if (assetIndex < 0 || assetIndex >= assets.Count || assets[assetIndex] is not JsonValue asset || !asset.TryGetValue<string>(out string? assetName) || string.IsNullOrWhiteSpace(assetName))
                    continue;
                string path = Path.Combine(gameData.ProjectPath, "Assets", "Sounds", assetName);
                if (!File.Exists(path))
                    continue;
                IAnimationAudioPlayback? player = AnimationAudioPlayback.Create(path, time - start);
                if (player is not null)
                    soundPlayers[playerKey] = player;
            }
        }
        foreach ((int Track, int Segment) playerKey in soundPlayers.Keys.Where(playerKey => !active.Contains(playerKey)).ToArray())
        {
            soundPlayers[playerKey].Dispose();
            soundPlayers.Remove(playerKey);
        }
    }

    private void onEditorKeyDown(object? sender, KeyEventArgs args)
    {
        if (isTextInputFocused())
            return;
        bool control = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
        bool alt = args.KeyModifiers.HasFlag(KeyModifiers.Alt);
        if (args.Key == InputKey.F2)
        {
            if (timeline.SelectedTimeTag >= 0)
                _ = renameTimeTag(timeline.SelectedTimeTag);
            else
            {
                nameBox.Focus();
                nameBox.SelectAll();
            }
            args.Handled = true;
        }
        else if (control && args.Key == InputKey.C)
        {
            segmentClipboard = timeline.GetSelectedSegmentData();
            args.Handled = true;
        }
        else if (control && args.Key == InputKey.X)
        {
            segmentClipboard = timeline.GetSelectedSegmentData();
            if (segmentClipboard is not null)
                timeline.DeleteSelectedSegment();
            args.Handled = true;
        }
        else if (control && args.Key == InputKey.V && segmentClipboard is { } clipboard)
        {
            timeline.InsertSegmentAt(clipboard.Track, (JsonObject)clipboard.Segment.DeepClone(), timeline.CurrentTime);
            args.Handled = true;
        }
        else if (control && args.Key == InputKey.D)
        {
            timeline.DuplicateSelectedSegment();
            args.Handled = true;
        }
        else if (args.Key == InputKey.Space)
        {
            togglePlayback();
            args.Handled = true;
        }
        else if (args.Key == InputKey.Home)
        {
            stopPlayback();
            timeline.SetTime(0);
            args.Handled = true;
        }
        else if (args.Key == InputKey.End)
        {
            stopPlayback();
            timeline.SetTime(maxTime());
            args.Handled = true;
        }
        else if (alt && args.Key is InputKey.Left or InputKey.Right)
        {
            timeline.NudgeSelectedSegment(args.Key == InputKey.Left ? -1 : 1);
            args.Handled = true;
        }
        else if (args.Key is InputKey.Left or InputKey.Right or InputKey.OemComma or InputKey.OemPeriod)
        {
            stopPlayback();
            int direction = args.Key is InputKey.Left or InputKey.OemComma ? -1 : 1;
            timeline.SetTime(Math.Clamp(timeline.CurrentTime + direction / (double)frameRate(), 0, maxTime()));
            args.Handled = true;
        }
        else if (args.Key is InputKey.Add or InputKey.OemPlus or InputKey.OemMinus or InputKey.Subtract)
        {
            timeline.AdjustZoom(args.Key is InputKey.Add or InputKey.OemPlus ? 0.1 : -0.1);
            args.Handled = true;
        }
    }

    private bool isTextInputFocused()
    {
        return TopLevel.GetTopLevel(this)?.FocusManager?.GetFocusedElement() is TextBox or ComboBox;
    }

    private double maxTime()
    {
        double maximum = 0;
        foreach (JsonNode? lineNode in getTimeLines())
        {
            if (lineNode is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            foreach (JsonNode? segmentNode in segments)
            {
                if (segmentNode is JsonObject segment)
                    maximum = Math.Max(maximum, number((segment["endFrame"] as JsonObject)?["time"]));
            }
        }
        return maximum;
    }

    private void refreshEditor()
    {
        loadingInspector = true;
        nameBox.Text = data["name"]?.GetValue<string>() ?? key;
        fpsBox.SelectedItem = frameRate().ToString(CultureInfo.InvariantCulture);
        loadingInspector = false;
        refreshAssets();
        timeline.Refresh();
        preview.Refresh();
        loadInspector();
    }

    private void refreshAssets()
    {
        int assetCount = getAssets().Count;
        selectedAssetIndexes.RemoveWhere(index => index < 0 || index >= assetCount);
        if (assetSelectionAnchor >= assetCount)
            assetSelectionAnchor = -1;
        assetGrid.Children.Clear();
        JsonArray assets = getAssets();
        assetGrid.RowDefinitions.Clear();
        int rowCount = (assets.Count + 2) / 3;
        for (int row = 0; row < rowCount; row += 1)
            assetGrid.RowDefinitions.Add(new RowDefinition(new GridLength(64)));
        for (int index = 0; index < assets.Count; index += 1)
        {
            string assetName = assets[index]?.GetValue<string>() ?? string.Empty;
            Border item = createAssetItem(index, assetName);
            Grid.SetRow(item, index / 3);
            Grid.SetColumn(item, index % 3);
            assetGrid.Children.Add(item);
        }
    }

    private Border createAssetItem(int index, string assetName)
    {
        Border item = new()
        {
            Width = 64,
            Height = 64,
            BorderThickness = new Thickness(selectedAssetIndexes.Contains(index) ? 3 : 1),
            BorderBrush = new SolidColorBrush(Color.Parse(selectedAssetIndexes.Contains(index) ? "#8ab4f8" : "#555555")),
            Background = new SolidColorBrush(Color.Parse("#333333")),
            Margin = new Thickness(0),
            Tag = index,
        };
        ToolTip.SetTip(item, assetName);
        PointerPressedEventArgs? assetDragPress = null;
        Point? assetDragStart = null;
        bool collapseSelectionOnRelease = false;
        item.PointerPressed += (_, args) =>
        {
            if (args.GetCurrentPoint(item).Properties.IsLeftButtonPressed)
            {
                bool primary = EditorShortcuts.HasPrimaryModifier(args.KeyModifiers);
                bool shift = args.KeyModifiers.HasFlag(KeyModifiers.Shift);
                collapseSelectionOnRelease = false;
                if (shift)
                {
                    int anchor = assetSelectionAnchor >= 0 ? assetSelectionAnchor : index;
                    if (assetSelectionAnchor < 0)
                        assetSelectionAnchor = anchor;
                    if (!primary)
                        selectedAssetIndexes.Clear();
                    for (int selectedIndex = Math.Min(anchor, index); selectedIndex <= Math.Max(anchor, index); selectedIndex += 1)
                        selectedAssetIndexes.Add(selectedIndex);
                }
                else if (primary)
                {
                    if (!selectedAssetIndexes.Add(index))
                        selectedAssetIndexes.Remove(index);
                    assetSelectionAnchor = index;
                }
                else if (selectedAssetIndexes.Contains(index) && selectedAssetIndexes.Count > 1)
                    collapseSelectionOnRelease = true;
                else
                {
                    selectedAssetIndexes.Clear();
                    selectedAssetIndexes.Add(index);
                    assetSelectionAnchor = index;
                }
                refreshAssetSelection();
                assetDragPress = args;
                assetDragStart = args.GetPosition(item);
            }
        };
        item.PointerMoved += async (_, args) =>
        {
            if (assetDragPress is null || assetDragStart is not Point start)
                return;
            if (!args.GetCurrentPoint(item).Properties.IsLeftButtonPressed)
            {
                assetDragPress = null;
                assetDragStart = null;
                return;
            }
            Point current = args.GetPosition(item);
            if (Math.Abs(current.X - start.X) + Math.Abs(current.Y - start.Y) < 8)
                return;
            if (!selectedAssetIndexes.Contains(index))
            {
                assetDragPress = null;
                assetDragStart = null;
                return;
            }
            PointerPressedEventArgs press = assetDragPress;
            assetDragPress = null;
            assetDragStart = null;
            collapseSelectionOnRelease = false;
            JsonArray indexes = new();
            foreach (int selectedIndex in selectedAssetIndexes.Order())
                indexes.Add(selectedIndex);
            DataTransfer dragData = new();
            dragData.Add(DataTransferItem.CreateText(AnimationTimeline.AssetDragPrefix + indexes.ToJsonString()));
            await DragDrop.DoDragDropAsync(press, dragData, DragDropEffects.Copy);
        };
        item.PointerReleased += (_, _) =>
        {
            if (collapseSelectionOnRelease && assetDragPress is not null)
            {
                selectedAssetIndexes.Clear();
                selectedAssetIndexes.Add(index);
                assetSelectionAnchor = index;
                refreshAssetSelection();
            }
            collapseSelectionOnRelease = false;
            assetDragPress = null;
            assetDragStart = null;
        };
        string extension = Path.GetExtension(assetName);
        if (!isAudioAsset(assetName) && new[] { ".png", ".jpg", ".bmp" }.Contains(extension, StringComparer.OrdinalIgnoreCase))
        {
            string path = Path.Combine(gameData.ProjectPath, "Assets", "Animations", assetName);
            if (File.Exists(path))
            {
                item.Child = new Image { Source = new Bitmap(path), Stretch = Stretch.Uniform };
            }
            else
                item.Child = createAssetText("Missing");
        }
        else if (isAudioAsset(assetName))
        {
            string path = Path.Combine(gameData.ProjectPath, "Assets", "Sounds", assetName);
            if (File.Exists(path))
            {
                item.Background = new SolidColorBrush(Color.Parse("#442222"));
                item.Child = createAssetText("Audio");
            }
            else
                item.Child = createAssetText("Missing");
        }
        else
            item.Child = createAssetText("Unknown");
        return item;
    }

    private void refreshAssetSelection()
    {
        foreach (Control child in assetGrid.Children)
        {
            if (child is not Border item || item.Tag is not int index)
                continue;
            bool selected = selectedAssetIndexes.Contains(index);
            item.BorderThickness = new Thickness(selected ? 3 : 1);
            item.BorderBrush = new SolidColorBrush(Color.Parse(selected ? "#8ab4f8" : "#555555"));
        }
    }

    private static TextBlock createAssetText(string text) => new()
    {
        Text = text,
        HorizontalAlignment = HorizontalAlignment.Center,
        VerticalAlignment = VerticalAlignment.Center,
    };

    private IEnumerable<TextBox> inspectorFields()
    {
        return [startTime, startX, startY, startRotation, startScaleX, startScaleY, endTime, endX, endY, endRotation, endScaleX, endScaleY];
    }

    private JsonArray getAssets()
    {
        if (data["assets"] is not JsonArray assets)
        {
            assets = new JsonArray();
            data["assets"] = assets;
        }
        return assets;
    }

    private JsonArray getTimeLines()
    {
        if (data["timeLines"] is not JsonArray lines)
        {
            lines = new JsonArray();
            data["timeLines"] = lines;
        }
        return lines;
    }

    private JsonObject? getSelectedSegment()
    {
        JsonArray lines = getTimeLines();
        if (selectedTrack < 0 || selectedTrack >= lines.Count || lines[selectedTrack] is not JsonObject line || line["timeSegments"] is not JsonArray segments
            || selectedSegment < 0 || selectedSegment >= segments.Count)
            return null;
        return segments[selectedSegment] as JsonObject;
    }

    private int frameRate() => Math.Max(1, (int)number(data["frameRate"], 30));

    private static JsonObject createFrame(double time) => new()
    {
        ["time"] = time,
        ["position"] = new JsonArray(0.0, 0.0),
        ["rotation"] = 0.0,
        ["scale"] = new JsonArray(1.0, 1.0),
    };

    private static JsonObject ensureFrame(JsonObject segment, string property)
    {
        if (segment[property] is JsonObject frame)
            return frame;
        frame = createFrame(0.0);
        segment[property] = frame;
        return frame;
    }

    private static void setFrameFields(JsonObject frame, TextBox time, TextBox x, TextBox y, TextBox rotation, TextBox scaleX, TextBox scaleY)
    {
        JsonArray position = frame["position"] as JsonArray ?? new JsonArray(0.0, 0.0);
        JsonArray scale = frame["scale"] as JsonArray ?? new JsonArray(1.0, 1.0);
        time.Text = number(frame["time"]).ToString(CultureInfo.InvariantCulture);
        x.Text = number(position.ElementAtOrDefault(0)).ToString(CultureInfo.InvariantCulture);
        y.Text = number(position.ElementAtOrDefault(1)).ToString(CultureInfo.InvariantCulture);
        rotation.Text = number(frame["rotation"]).ToString(CultureInfo.InvariantCulture);
        scaleX.Text = number(scale.ElementAtOrDefault(0), 1).ToString(CultureInfo.InvariantCulture);
        scaleY.Text = number(scale.ElementAtOrDefault(1), 1).ToString(CultureInfo.InvariantCulture);
    }

    private static bool tryReadFrame(TextBox time, TextBox x, TextBox y, TextBox rotation, TextBox scaleX, TextBox scaleY, out JsonObject frame)
    {
        frame = createFrame(0);
        if (!tryNumber(time.Text, out double frameTime) || !tryNumber(x.Text, out double positionX) || !tryNumber(y.Text, out double positionY)
            || !tryNumber(rotation.Text, out double frameRotation) || !tryNumber(scaleX.Text, out double frameScaleX) || !tryNumber(scaleY.Text, out double frameScaleY))
            return false;
        frame["time"] = Math.Max(0, frameTime);
        frame["position"] = new JsonArray(positionX, positionY);
        frame["rotation"] = frameRotation;
        frame["scale"] = new JsonArray(frameScaleX, frameScaleY);
        return true;
    }

    private static bool tryNumber(string? value, out double result) => double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out result);
    internal static double number(JsonNode? node, double fallback = 0)
    {
        if (node is not JsonValue value)
            return fallback;
        if (value.TryGetValue<double>(out double doubleValue))
            return doubleValue;
        if (value.TryGetValue<float>(out float floatValue))
            return floatValue;
        if (value.TryGetValue<int>(out int intValue))
            return intValue;
        if (value.TryGetValue<long>(out long longValue))
            return longValue;
        if (value.TryGetValue<decimal>(out decimal decimalValue))
            return (double)decimalValue;
        if (value.TryGetValue<JsonElement>(out JsonElement element)
            && element.ValueKind == JsonValueKind.Number
            && element.TryGetDouble(out double elementValue))
        {
            return elementValue;
        }
        return fallback;
    }
    internal static bool isAudioAsset(string name) => new[] { ".wav", ".ogg", ".mp3" }.Contains(Path.GetExtension(name), StringComparer.OrdinalIgnoreCase);
}

public sealed class AnimationTimeline : Control
{
    internal const string AssetDragPrefix = "ludork-animation-assets:";
    private const double SegmentMarqueeThreshold = 4;
    private readonly string projectPath;
    private readonly Func<JsonObject> getData;
    private readonly HashSet<(int Track, int Segment)> segmentSelectionBeforeMarquee = [];
    private readonly HashSet<(int Track, int Segment)> selectedSegments = [];
    private Point dragStart;
    private Point segmentMarqueeStart;
    private Rect segmentMarqueeRect;
    private double originalStart;
    private double originalEnd;
    private JsonObject? draggedTimeTag;
    private double timeTagPointerOffset;
    private bool timeTagMoved;
    private bool segmentMarqueeActive;
    private bool segmentMarqueeAdditive;
    private bool segmentMarqueeToggle;
    private int dragMode;
    private const int ScrubDragMode = 4;
    private const int TimeTagDragMode = 5;
    private const int SegmentMarqueeDragMode = 6;
    private const double HeaderHeight = 28;
    private const double TrackHeight = 38;
    private const double BasePixelsPerSecond = 300;
    private readonly EditorZoomInput zoomInput = new();
    private ScrollViewer? hostScrollViewer;
    private double zoom = 1.0;
    private TimelineZoomAnchor? pendingZoomAnchor;

    public AnimationTimeline(string projectPath, Func<JsonObject> getData)
    {
        this.projectPath = projectPath;
        this.getData = getData;
        Focusable = true;
        DragDrop.SetAllowDrop(this, true);
        AddHandler(DragDrop.DragOverEvent, onDragOver);
        AddHandler(DragDrop.DropEvent, onDrop);
        AddHandler(
            PointerCaptureLostEvent,
            onPointerCaptureLost,
            RoutingStrategies.Tunnel
        );
        PointerTouchPadGestureMagnify += onPointerTouchPadGestureMagnify;
    }

    public event Action<int, int>? SegmentSelected;
    public event Action? SegmentChanged;
    public event Action? TimeTagChanged;
    public event Action<int>? TimeTagRenameRequested;
    public event Action<double>? TimeChanged;
    public event Action<double>? ZoomChanged;
    public double CurrentTime { get; private set; }
    public int SelectedTrack { get; set; } = -1;
    public int SelectedSegment { get; set; } = -1;
    public int SelectedTimeTag { get; private set; } = -1;

    public double PixelsPerSecond => BasePixelsPerSecond * zoom;

    public void Refresh()
    {
        JsonObject? selectedTimeTag = GetTimeTag(SelectedTimeTag);
        SelectedTimeTag = sortTimeTags(selectedTimeTag);
        selectedSegments.IntersectWith(validSelectedSegments());
        updatePrimarySegmentSelection();
        updateCanvasSize();
        InvalidateVisual();
    }

    public void SetZoom(double value)
    {
        setZoom(value, null);
    }

    public void AdjustZoom(double delta) => SetZoom(zoom + delta);

    public void ClearTimeTagSelection()
    {
        SelectedTimeTag = -1;
        InvalidateVisual();
    }

    public void SetSegmentSelection(int track, int segment)
    {
        selectedSegments.Clear();
        if (segmentAt(track, segment) is not null)
            selectedSegments.Add((track, segment));
        updatePrimarySegmentSelection();
        InvalidateVisual();
    }

    public void AddTimeTag(string tag)
    {
        JsonObject timeTag = new()
        {
            ["tag"] = tag,
            ["time"] = CurrentTime,
        };
        timeTags().Add(timeTag);
        SelectedTimeTag = sortTimeTags(timeTag);
        TimeTagChanged?.Invoke();
        Refresh();
    }

    public JsonObject? GetTimeTag(int index)
    {
        JsonArray tags = timeTags();
        return index >= 0 && index < tags.Count ? tags[index] as JsonObject : null;
    }

    public void RenameTimeTag(int index, string tag)
    {
        if (GetTimeTag(index) is not JsonObject timeTag)
            return;
        timeTag["tag"] = tag;
        SelectedTimeTag = index;
        TimeTagChanged?.Invoke();
        Refresh();
    }

    public bool DeleteSelectedTimeTag()
    {
        JsonArray tags = timeTags();
        if (SelectedTimeTag < 0 || SelectedTimeTag >= tags.Count)
            return false;
        tags.RemoveAt(SelectedTimeTag);
        SelectedTimeTag = -1;
        TimeTagChanged?.Invoke();
        Refresh();
        return true;
    }

    public int FindAvailableTrack(double start, double end)
    {
        JsonArray tracks = lines();
        for (int track = 0; track < tracks.Count; track += 1)
        {
            if (!overlaps(track, start, end, -1))
                return track;
        }
        return tracks.Count;
    }

    public (int Track, JsonObject Segment)? GetSelectedSegmentData()
    {
        return selectedSegments.Count == 1
            && segmentAt(SelectedTrack, SelectedSegment) is JsonObject segment
            ? (SelectedTrack, (JsonObject)segment.DeepClone())
            : null;
    }

    public bool DeleteSelectedSegment()
    {
        HashSet<(int Track, int Segment)> selection = validSelectedSegments();
        if (selection.Count == 0)
            return false;
        JsonArray tracks = lines();
        foreach (IGrouping<int, (int Track, int Segment)> group in selection
            .GroupBy(item => item.Track)
            .OrderByDescending(group => group.Key))
        {
            if (tracks[group.Key] is not JsonObject line
                || line["timeSegments"] is not JsonArray segments)
            {
                continue;
            }
            foreach ((int Track, int Segment) item in group.OrderByDescending(item => item.Segment))
                segments.RemoveAt(item.Segment);
        }
        selectedSegments.Clear();
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(-1, -1);
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    public bool InsertSegmentAt(int track, JsonObject segment, double startTime)
    {
        JsonArray tracks = lines();
        while (tracks.Count <= track)
            tracks.Add(new JsonObject { ["timeSegments"] = new JsonArray() });
        JsonArray segments = ((JsonObject)tracks[track]!)["timeSegments"]!.AsArray();
        JsonObject startFrame = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject endFrame = segment["endFrame"] as JsonObject ?? new JsonObject();
        double duration = Math.Max(minimumDuration(segment), AnimationEditor.number(endFrame["time"]) - AnimationEditor.number(startFrame["time"]));
        double start = clampInsertStart(track, -1, Math.Max(0, snap(startTime)), duration);
        if (overlaps(track, start, start + duration, -1))
            return false;
        startFrame["time"] = start;
        endFrame["time"] = start + duration;
        segment["startFrame"] = startFrame;
        segment["endFrame"] = endFrame;
        segments.Add(segment);
        SetSegmentSelection(track, segments.Count - 1);
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    public bool DuplicateSelectedSegment()
    {
        if (selectedSegments.Count != 1
            || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject source)
            return false;
        double end = AnimationEditor.number((source["endFrame"] as JsonObject)?["time"]);
        return InsertSegmentAt(SelectedTrack, (JsonObject)source.DeepClone(), end);
    }

    public bool NudgeSelectedSegment(int frameDelta)
    {
        if (selectedSegments.Count != 1
            || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return false;
        JsonObject startFrame = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject endFrame = segment["endFrame"] as JsonObject ?? new JsonObject();
        double start = AnimationEditor.number(startFrame["time"]);
        double end = AnimationEditor.number(endFrame["time"]);
        double duration = end - start;
        double savedStart = originalStart;
        double savedEnd = originalEnd;
        originalStart = start;
        originalEnd = end;
        (double left, double right) = findBounds(SelectedTrack, SelectedSegment);
        originalStart = savedStart;
        originalEnd = savedEnd;
        double next = Math.Max(Math.Max(0, left), Math.Min(snap(start + frameDelta / (double)frameRate()), right - duration));
        if (Math.Abs(next - start) < 0.0001)
            return false;
        startFrame["time"] = next;
        endFrame["time"] = next + duration;
        SegmentChanged?.Invoke();
        Refresh();
        return true;
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#292929")), bounds);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#383838")), new Rect(0, 0, bounds.Width, HeaderHeight));
        Pen gridPen = new(new SolidColorBrush(Color.Parse("#505050")), 1);
        for (double second = 0; second * PixelsPerSecond < bounds.Width; second += 1)
            context.DrawLine(gridPen, new Point(second * PixelsPerSecond, 0), new Point(second * PixelsPerSecond, bounds.Height));

        JsonArray tracks = lines();
        for (int track = 0; track < Math.Max(5, tracks.Count); track += 1)
        {
            double y = HeaderHeight + track * TrackHeight;
            context.FillRectangle(new SolidColorBrush(Color.Parse(track % 2 == 0 ? "#303030" : "#2c2c2c")), new Rect(0, y, bounds.Width, TrackHeight));
            if (track >= tracks.Count || tracks[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int segmentIndex = 0; segmentIndex < segments.Count; segmentIndex += 1)
            {
                if (segments[segmentIndex] is not JsonObject segment)
                    continue;
                Rect rect = segmentRect(track, segment);
                bool selected = selectedSegments.Contains((track, segmentIndex));
                bool sound = segment["type"]?.GetValue<string>() == "sound";
                context.FillRectangle(new SolidColorBrush(Color.Parse(selected ? "#8ab4f8" : sound ? "#76573b" : "#4f89b8")), rect, 4);
                context.DrawRectangle(selected ? new Pen(Brushes.White, 2) : new Pen(new SolidColorBrush(Color.Parse("#a9cce8")), 1), rect, 4);
            }
        }
        if (segmentMarqueeActive)
        {
            context.FillRectangle(
                new SolidColorBrush(Color.Parse("#338ab4f8")),
                segmentMarqueeRect
            );
            context.DrawRectangle(
                new Pen(new SolidColorBrush(Color.Parse("#8ab4f8")), 1),
                segmentMarqueeRect
            );
        }
        JsonArray tags = timeTags();
        using (context.PushClip(new Rect(0, 0, bounds.Width, HeaderHeight)))
        {
            for (int index = 0; index < tags.Count; index += 1)
            {
                if (tags[index] is not JsonObject timeTag)
                    continue;
                double actualX = AnimationEditor.number(timeTag["time"]) * PixelsPerSecond;
                double markerX = timeTagDisplayX(index);
                bool selected = index == SelectedTimeTag;
                SolidColorBrush markerBrush = new(Color.Parse(selected ? "#ffd166" : "#5ad1c4"));
                if (Math.Abs(markerX - actualX) > 0.01)
                    context.DrawLine(new Pen(markerBrush, 1), new Point(actualX, HeaderHeight - 1), new Point(markerX, HeaderHeight - 1));
                StreamGeometry marker = new();
                using (StreamGeometryContext geometry = marker.Open())
                {
                    geometry.BeginFigure(new Point(markerX, HeaderHeight), true);
                    geometry.LineTo(new Point(markerX - 6, HeaderHeight - 9));
                    geometry.LineTo(new Point(markerX + 6, HeaderHeight - 9));
                    geometry.EndFigure(true);
                }
                context.DrawGeometry(markerBrush, selected ? new Pen(Brushes.White, 1) : null, marker);
                string tag = timeTag["tag"]?.GetValue<string>() ?? string.Empty;
                FormattedText label = new(
                    tag,
                    CultureInfo.CurrentUICulture,
                    FlowDirection.LeftToRight,
                    Typeface.Default,
                    10,
                    markerBrush
                );
                context.DrawText(label, new Point(markerX + 7, 2));
            }
        }
        if (SelectedTimeTag >= 0 && GetTimeTag(SelectedTimeTag) is JsonObject selectedTimeTag)
        {
            double timeTagX = AnimationEditor.number(selectedTimeTag["time"]) * PixelsPerSecond;
            context.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#66ffd166")), 1), new Point(timeTagX, HeaderHeight), new Point(timeTagX, bounds.Height));
        }
        double playhead = CurrentTime * PixelsPerSecond;
        SolidColorBrush playheadBrush = new(Color.Parse("#ff5c5c"));
        context.DrawLine(new Pen(playheadBrush, 1), new Point(playhead, 0), new Point(playhead, bounds.Height));
        StreamGeometry head = new();
        using (StreamGeometryContext geometry = head.Open())
        {
            geometry.BeginFigure(new Point(playhead, 18), true);
            geometry.LineTo(new Point(playhead - 6, 12));
            geometry.LineTo(new Point(playhead - 6, 0));
            geometry.LineTo(new Point(playhead + 6, 0));
            geometry.LineTo(new Point(playhead + 6, 12));
            geometry.EndFigure(true);
        }
        context.DrawGeometry(playheadBrush, null, head);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        Point position = e.GetPosition(this);
        if (e.GetCurrentPoint(this).Properties.IsRightButtonPressed)
        {
            int timeTag = hitTimeTag(position);
            if (timeTag >= 0)
            {
                Focus();
                SelectedTimeTag = timeTag;
                selectedSegments.Clear();
                updatePrimarySegmentSelection();
                SegmentSelected?.Invoke(-1, -1);
                showTimeTagContextMenu();
                InvalidateVisual();
                e.Handled = true;
            }
            return;
        }
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Focus();
        int selectedTimeTag = hitTimeTag(position);
        if (selectedTimeTag >= 0)
        {
            SelectedTimeTag = selectedTimeTag;
            selectedSegments.Clear();
            updatePrimarySegmentSelection();
            SegmentSelected?.Invoke(-1, -1);
            if (e.ClickCount == 2)
            {
                TimeTagRenameRequested?.Invoke(selectedTimeTag);
                e.Handled = true;
                InvalidateVisual();
                return;
            }
            draggedTimeTag = GetTimeTag(selectedTimeTag);
            double tagTime = AnimationEditor.number(draggedTimeTag?["time"]);
            timeTagPointerOffset = position.X - tagTime * PixelsPerSecond;
            timeTagMoved = false;
            dragMode = TimeTagDragMode;
            dragStart = position;
            SetTime(tagTime);
            e.Pointer.Capture(this);
            InvalidateVisual();
            return;
        }
        SelectedTimeTag = -1;
        (int track, int segment, int mode) = hitTest(position);
        if (segment >= 0)
        {
            selectSegmentFromPointer(track, segment, e.KeyModifiers);
            if (selectedSegments.Count == 1 && selectedSegments.Contains((track, segment)))
            {
                dragMode = mode;
                dragStart = position;
                JsonObject current = segmentAt(track, segment)!;
                originalStart = AnimationEditor.number((current["startFrame"] as JsonObject)?["time"]);
                originalEnd = AnimationEditor.number((current["endFrame"] as JsonObject)?["time"]);
            }
            else
                dragMode = 0;
        }
        else if (position.Y < HeaderHeight)
        {
            selectedSegments.Clear();
            updatePrimarySegmentSelection();
            SegmentSelected?.Invoke(-1, -1);
            dragMode = ScrubDragMode;
            dragStart = position;
            SetTime(position.X / PixelsPerSecond);
        }
        else
        {
            beginSegmentMarquee(position, e.KeyModifiers);
        }
        e.Pointer.Capture(this);
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed is false)
        {
            if (dragMode == SegmentMarqueeDragMode)
            {
                completeSegmentMarquee(false);
                e.Pointer.Capture(null);
            }
            return;
        }
        if (dragMode == 0)
            return;
        if (dragMode == SegmentMarqueeDragMode)
        {
            Point position = e.GetPosition(this);
            if (!segmentMarqueeActive
                && Math.Abs(position.X - segmentMarqueeStart.X) < SegmentMarqueeThreshold
                && Math.Abs(position.Y - segmentMarqueeStart.Y) < SegmentMarqueeThreshold)
            {
                return;
            }
            segmentMarqueeActive = true;
            updateSegmentMarquee(position);
            return;
        }
        if (dragMode == TimeTagDragMode)
        {
            if (draggedTimeTag is not null)
            {
                double next = Math.Max(0, snap((e.GetPosition(this).X - timeTagPointerOffset) / PixelsPerSecond));
                double current = AnimationEditor.number(draggedTimeTag["time"]);
                if (Math.Abs(next - current) > 0.000001)
                {
                    draggedTimeTag["time"] = next;
                    timeTagMoved = true;
                    SetTime(next);
                }
            }
            return;
        }
        if (dragMode == ScrubDragMode)
        {
            SetTime(e.GetPosition(this).X / PixelsPerSecond);
            return;
        }
        if (segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return;
        double delta = (e.GetPosition(this).X - dragStart.X) / PixelsPerSecond;
        double start = originalStart;
        double end = originalEnd;
        double minimum = segment["type"]?.GetValue<string>() == "sound" ? 1.0 / frameRate() : 0.05;
        double maximum = segment["type"]?.GetValue<string>() == "sound"
            ? AnimationEditor.number(segment["originalDuration"], double.PositiveInfinity)
            : double.PositiveInfinity;
        if (dragMode == 1)
        {
            double duration = originalEnd - originalStart;
            double potentialStart = Math.Max(0, snap(originalStart + delta));
            (double left, double right) = findBounds(SelectedTrack, SelectedSegment);
            start = Math.Max(Math.Max(0, left), Math.Min(potentialStart, right - duration));
            end = start + duration;
        }
        else if (dragMode == 2)
        {
            (double left, _) = findBounds(SelectedTrack, SelectedSegment);
            start = Math.Max(Math.Max(left, end - maximum), Math.Min(snap(originalStart + delta), end - minimum));
        }
        else
        {
            (_, double right) = findBounds(SelectedTrack, SelectedSegment);
            end = Math.Min(Math.Min(right, start + maximum), Math.Max(start + minimum, snap(originalEnd + delta)));
        }
        (segment["startFrame"] as JsonObject)!["time"] = start;
        (segment["endFrame"] as JsonObject)!["time"] = end;
        InvalidateVisual();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        if (dragMode == SegmentMarqueeDragMode)
        {
            if (segmentMarqueeActive)
                updateSegmentMarquee(e.GetPosition(this));
            completeSegmentMarquee(true);
        }
        else if (dragMode == TimeTagDragMode)
        {
            if (timeTagMoved && draggedTimeTag is not null)
            {
                SelectedTimeTag = sortTimeTags(draggedTimeTag);
                TimeTagChanged?.Invoke();
                Refresh();
            }
            draggedTimeTag = null;
            timeTagMoved = false;
        }
        else if (dragMode != 0 && dragMode != ScrubDragMode)
            SegmentChanged?.Invoke();
        dragMode = 0;
        e.Pointer.Capture(null);
    }

    private void beginSegmentMarquee(Point position, KeyModifiers modifiers)
    {
        segmentMarqueeStart = position;
        segmentMarqueeRect = new Rect(position, new Size());
        segmentMarqueeActive = false;
        segmentMarqueeAdditive = modifiers.HasFlag(KeyModifiers.Shift);
        segmentMarqueeToggle = !segmentMarqueeAdditive
            && EditorShortcuts.HasPrimaryModifier(modifiers);
        segmentSelectionBeforeMarquee.Clear();
        segmentSelectionBeforeMarquee.UnionWith(selectedSegments);
        SelectedTimeTag = -1;
        dragMode = SegmentMarqueeDragMode;
    }

    private void updateSegmentMarquee(Point position)
    {
        Rect trackBounds = new(
            0,
            HeaderHeight,
            Bounds.Width,
            Math.Max(0, Bounds.Height - HeaderHeight)
        );
        segmentMarqueeRect = AnimationEditor.createMarqueeRect(
            segmentMarqueeStart,
            position,
            trackBounds
        );
        List<(int Track, int Segment)> hits = [];
        JsonArray tracks = lines();
        for (int track = 0; track < tracks.Count; track += 1)
        {
            if (tracks[track] is not JsonObject line
                || line["timeSegments"] is not JsonArray segments)
            {
                continue;
            }
            for (int segment = 0; segment < segments.Count; segment += 1)
            {
                if (segments[segment] is JsonObject data
                    && AnimationEditor.rectsOverlap(segmentMarqueeRect, segmentRect(track, data)))
                {
                    hits.Add((track, segment));
                }
            }
        }
        HashSet<(int Track, int Segment)> selection = AnimationEditor.buildMarqueeSelection(
            segmentSelectionBeforeMarquee,
            hits,
            segmentMarqueeAdditive,
            segmentMarqueeToggle
        );
        selectedSegments.Clear();
        selectedSegments.UnionWith(selection);
        updatePrimarySegmentSelection();
        InvalidateVisual();
    }

    private void completeSegmentMarquee(bool clearOnBlank)
    {
        if (dragMode != SegmentMarqueeDragMode)
            return;
        if (!segmentMarqueeActive
            && clearOnBlank
            && !segmentMarqueeAdditive
            && !segmentMarqueeToggle)
        {
            selectedSegments.Clear();
        }
        segmentMarqueeActive = false;
        segmentMarqueeAdditive = false;
        segmentMarqueeToggle = false;
        segmentSelectionBeforeMarquee.Clear();
        segmentMarqueeRect = default;
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        dragMode = 0;
        InvalidateVisual();
    }

    private void selectSegmentFromPointer(
        int track,
        int segment,
        KeyModifiers modifiers
    )
    {
        bool additive = modifiers.HasFlag(KeyModifiers.Shift);
        bool toggle = !additive && EditorShortcuts.HasPrimaryModifier(modifiers);
        (int Track, int Segment) selection = (track, segment);
        if (toggle)
        {
            if (!selectedSegments.Add(selection))
                selectedSegments.Remove(selection);
        }
        else if (additive)
            selectedSegments.Add(selection);
        else
        {
            selectedSegments.Clear();
            selectedSegments.Add(selection);
        }
        updatePrimarySegmentSelection();
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
    }

    private void updatePrimarySegmentSelection()
    {
        if (selectedSegments.Count == 1)
        {
            (int Track, int Segment) selection = selectedSegments.Single();
            SelectedTrack = selection.Track;
            SelectedSegment = selection.Segment;
            return;
        }
        SelectedTrack = -1;
        SelectedSegment = -1;
    }

    private HashSet<(int Track, int Segment)> validSelectedSegments()
    {
        HashSet<(int Track, int Segment)> selection = [];
        foreach ((int Track, int Segment) item in selectedSegments)
        {
            if (segmentAt(item.Track, item.Segment) is not null)
                selection.Add(item);
        }
        return selection;
    }

    private Rect segmentRect(int track, JsonObject segment)
    {
        double start = AnimationEditor.number(
            (segment["startFrame"] as JsonObject)?["time"]
        );
        double end = AnimationEditor.number(
            (segment["endFrame"] as JsonObject)?["time"]
        );
        return new Rect(
            start * PixelsPerSecond,
            HeaderHeight + track * TrackHeight + 5,
            Math.Max(3, (end - start) * PixelsPerSecond),
            TrackHeight - 10
        );
    }

    private void onPointerCaptureLost(object? sender, PointerCaptureLostEventArgs args)
    {
        if (dragMode == SegmentMarqueeDragMode)
            completeSegmentMarquee(false);
    }

    protected override void OnPointerWheelChanged(PointerWheelEventArgs e)
    {
        if (zoomInput.ShouldSuppressWheel())
        {
            e.Handled = true;
            return;
        }
        if (EditorZoomInput.ShouldZoomWheel(e.KeyModifiers, true)
            && e.Delta.Y != 0)
        {
            double nextZoom = zoom + (e.Delta.Y > 0 ? 0.1 : -0.1);
            setZoom(nextZoom, createZoomAnchor(e));
            e.Handled = true;
            return;
        }
        base.OnPointerWheelChanged(e);
    }

    protected override void OnAttachedToVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        LayoutUpdated += onLayoutUpdated;
        hostScrollViewer = this.FindAncestorOfType<ScrollViewer>();
    }

    protected override void OnDetachedFromVisualTree(
        VisualTreeAttachmentEventArgs args)
    {
        LayoutUpdated -= onLayoutUpdated;
        pendingZoomAnchor = null;
        hostScrollViewer = null;
        base.OnDetachedFromVisualTree(args);
    }

    private void onPointerTouchPadGestureMagnify(
        object? sender,
        PointerDeltaEventArgs args)
    {
        if (!EditorZoomInput.IsMacOS)
            return;
        zoomInput.MarkMagnify();
        double nextZoom = EditorZoomInput.ScaleByFactor(
            zoom,
            EditorZoomInput.GetMagnifyFactor(args.Delta.Y),
            0.2,
            5.0);
        setZoom(nextZoom, createZoomAnchor(args));
        args.Handled = true;
    }

    private TimelineZoomAnchor createZoomAnchor(PointerEventArgs args)
    {
        Point contentPoint = args.GetPosition(this);
        Point viewportPoint = hostScrollViewer is null
            ? contentPoint
            : args.GetPosition(hostScrollViewer);
        return new TimelineZoomAnchor(
            contentPoint.X / PixelsPerSecond,
            contentPoint.Y,
            viewportPoint);
    }

    private void setZoom(
        double value,
        TimelineZoomAnchor? anchor)
    {
        double nextZoom = Math.Clamp(value, 0.2, 5.0);
        if (Math.Abs(nextZoom - zoom) < 0.000001)
            return;
        zoom = nextZoom;
        pendingZoomAnchor = anchor;
        Refresh();
        ZoomChanged?.Invoke(zoom);
    }

    private void onLayoutUpdated(object? sender, EventArgs args)
    {
        if (pendingZoomAnchor is null)
            return;
        applyZoomAnchor();
    }

    private void applyZoomAnchor()
    {
        if (pendingZoomAnchor is not TimelineZoomAnchor anchor
            || hostScrollViewer is null)
        {
            pendingZoomAnchor = null;
            return;
        }
        pendingZoomAnchor = null;
        Point contentAnchor = new(
            anchor.Time * PixelsPerSecond,
            anchor.ContentY);
        hostScrollViewer.Offset = EditorZoomInput.GetAnchoredOffset(
            contentAnchor,
            anchor.ViewportPoint,
            hostScrollViewer.Extent,
            hostScrollViewer.Viewport);
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Key == Key.F2 && SelectedTimeTag >= 0)
        {
            TimeTagRenameRequested?.Invoke(SelectedTimeTag);
            e.Handled = true;
            return;
        }
        if (e.Key is Key.Delete or Key.Back)
        {
            if (SelectedTimeTag >= 0)
                DeleteSelectedTimeTag();
            else
                DeleteSelectedSegment();
            e.Handled = true;
            return;
        }
        base.OnKeyDown(e);
    }

    private void onDragOver(object? sender, DragEventArgs e)
    {
        string? payload = e.DataTransfer.TryGetText();
        Point position = e.GetPosition(this);
        int track = trackAt(position.Y);
        e.DragEffects = payload is not null && tryBuildAssetSegments(payload, track, position.X / PixelsPerSecond, out _)
            ? DragDropEffects.Copy : DragDropEffects.None;
        e.Handled = true;
    }

    private void onDrop(object? sender, DragEventArgs e)
    {
        if (e.DataTransfer.TryGetText() is not string payload)
            return;
        Point position = e.GetPosition(this);
        int track = trackAt(position.Y);
        if (tryBuildAssetSegments(payload, track, position.X / PixelsPerSecond, out List<JsonObject> segments))
            insertAssetSegments(track, segments);
        e.Handled = true;
    }

    public void SetTime(double time, bool snapToFrame = true)
    {
        CurrentTime = Math.Max(0, snapToFrame ? snap(time) : time);
        TimeChanged?.Invoke(CurrentTime);
        InvalidateVisual();
    }

    private (int track, int segment, int mode) hitTest(Point position)
    {
        int track = trackAt(position.Y);
        if (track < 0 || track >= lines().Count || lines()[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
            return (-1, -1, 0);
        double time = position.X / PixelsPerSecond;
        double handleWidth = 5.0 / PixelsPerSecond;
        for (int index = 0; index < segments.Count; index += 1)
        {
            if (segments[index] is not JsonObject segment)
                continue;
            double start = AnimationEditor.number((segment["startFrame"] as JsonObject)?["time"]);
            double end = AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]);
            if (time < start - handleWidth || time > end + handleWidth)
                continue;
            int mode = Math.Abs(time - start) <= handleWidth ? 2 : Math.Abs(time - end) <= handleWidth ? 3 : 1;
            return (track, index, mode);
        }
        return (-1, -1, 0);
    }

    private int hitTimeTag(Point position)
    {
        if (position.Y < 0 || position.Y >= HeaderHeight)
            return -1;
        JsonArray tags = timeTags();
        int result = -1;
        double distance = 8;
        for (int index = 0; index < tags.Count; index += 1)
        {
            if (tags[index] is not JsonObject)
                continue;
            double currentDistance = Math.Abs(position.X - timeTagDisplayX(index));
            if (currentDistance <= distance)
            {
                result = index;
                distance = currentDistance;
            }
        }
        return result;
    }

    private void showTimeTagContextMenu()
    {
        MenuItem rename = new() { Header = LocaleService.Get("RENAME_TIME_TAG") };
        rename.Click += (_, _) =>
        {
            if (SelectedTimeTag >= 0)
                TimeTagRenameRequested?.Invoke(SelectedTimeTag);
        };
        MenuItem delete = new() { Header = LocaleService.Get("DELETE") };
        delete.Click += (_, _) => DeleteSelectedTimeTag();
        ContextMenu menu = new() { ItemsSource = new object[] { rename, delete } };
        menu.Open(this);
    }

    private bool overlaps(int track, double start, double end, int ignored)
    {
        return overlaps(lines(), track, start, end, ignored);
    }

    private bool overlaps(JsonArray tracks, int track, double start, double end, int ignored)
    {
        if (track < 0 || track >= tracks.Count || tracks[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
            return false;
        for (int index = 0; index < segments.Count; index += 1)
        {
            if (index == ignored || segments[index] is not JsonObject segment)
                continue;
            double currentStart = AnimationEditor.number((segment["startFrame"] as JsonObject)?["time"]);
            double currentEnd = AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]);
            if (start < currentEnd && end > currentStart)
                return true;
        }
        return false;
    }

    private (double Left, double Right) findBounds(int track, int ignored)
    {
        double left = 0;
        double right = double.PositiveInfinity;
        if (track < 0 || track >= lines().Count || lines()[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
            return (left, right);
        foreach ((JsonNode? node, int index) in segments.Select((node, index) => (node, index)))
        {
            if (index == ignored || node is not JsonObject segment)
                continue;
            double start = AnimationEditor.number((segment["startFrame"] as JsonObject)?["time"]);
            double end = AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]);
            if (end <= originalStart + 0.0001)
                left = Math.Max(left, end);
            if (start >= originalEnd - 0.0001)
                right = Math.Min(right, start);
        }
        return (left, right);
    }

    private double clampInsertStart(int track, int ignored, double start, double duration)
    {
        double oldStart = originalStart;
        double oldEnd = originalEnd;
        originalStart = start;
        originalEnd = start + duration;
        (double left, double right) = findBounds(track, ignored);
        originalStart = oldStart;
        originalEnd = oldEnd;
        return Math.Max(left, Math.Min(start, right - duration));
    }

    private int trackAt(double y) => y < HeaderHeight ? -1 : (int)((y - HeaderHeight) / TrackHeight);

    private double minimumDuration(JsonObject segment) => segment["type"]?.GetValue<string>() == "sound" ? 1.0 / frameRate() : 0.05;
    private double defaultDuration(string asset)
    {
        if (!AnimationEditor.isAudioAsset(asset))
            return 0.05;
        double duration = AnimationAudioPlayback.GetDuration(Path.Combine(projectPath, "Assets", "Sounds", asset)) ?? 0.1;
        return duration > 0 ? duration : 0.1;
    }

    private bool tryGetExactAudioDuration(string asset, out double duration)
    {
        duration = AnimationAudioPlayback.GetDuration(Path.Combine(projectPath, "Assets", "Sounds", asset)) ?? 0;
        return duration > 0;
    }

    private bool tryBuildAssetSegments(string payload, int track, double dropTime, out List<JsonObject> segments)
    {
        segments = [];
        if (track < 0 || !tryResolveAssetIndexes(payload, out List<int> assetIndexes))
            return false;
        JsonArray assets = getData()["assets"] as JsonArray ?? new JsonArray();
        JsonArray existingTracks = getData()["timeLines"] as JsonArray ?? new JsonArray();
        bool batch = assetIndexes.Count > 1;
        double start = Math.Max(0, batch ? Math.Round(dropTime * 10.0) / 10.0 : snap(dropTime));
        double nextStart = start;
        foreach (int assetIndex in assetIndexes)
        {
            if (assetIndex < 0 || assetIndex >= assets.Count || assets[assetIndex] is not JsonValue assetValue
                || !assetValue.TryGetValue<string>(out string? asset) || string.IsNullOrWhiteSpace(asset))
            {
                segments.Clear();
                return false;
            }
            bool sound = AnimationEditor.isAudioAsset(asset);
            double duration;
            if (sound && batch)
            {
                if (!tryGetExactAudioDuration(asset, out duration))
                {
                    segments.Clear();
                    return false;
                }
            }
            else
                duration = sound ? defaultDuration(asset) : batch ? 0.1 : 0.05;
            double originalAudioDuration = duration;
            if (sound && !batch && track < existingTracks.Count && existingTracks[track] is JsonObject trackData
                && trackData["timeSegments"] is JsonArray existing)
            {
                foreach (JsonNode? node in existing)
                {
                    if (node is not JsonObject other)
                        continue;
                    double otherStart = AnimationEditor.number((other["startFrame"] as JsonObject)?["time"]);
                    if (otherStart > nextStart)
                        duration = Math.Min(duration, otherStart - nextStart);
                }
                originalAudioDuration = duration;
            }
            JsonObject segment = new()
            {
                ["type"] = sound ? "sound" : "frame",
                ["asset"] = assetIndex,
                ["startFrame"] = createFrame(nextStart),
                ["endFrame"] = createFrame(nextStart + duration),
            };
            if (sound)
                segment["originalDuration"] = originalAudioDuration;
            else
                segment["flipX"] = false;
            segments.Add(segment);
            nextStart += duration;
        }
        if (segments.Count == 0 || overlaps(existingTracks, track, start, nextStart, -1))
        {
            segments.Clear();
            return false;
        }
        return true;
    }

    private bool tryResolveAssetIndexes(string payload, out List<int> assetIndexes)
    {
        assetIndexes = [];
        JsonArray assets = getData()["assets"] as JsonArray ?? new JsonArray();
        if (!payload.StartsWith(AssetDragPrefix, StringComparison.Ordinal))
        {
            for (int index = 0; index < assets.Count; index += 1)
            {
                if (string.Equals(assets[index]?.GetValue<string>(), payload, StringComparison.Ordinal))
                {
                    assetIndexes.Add(index);
                    return true;
                }
            }
            return false;
        }
        string json = payload[AssetDragPrefix.Length..].Trim();
        if (json.Length < 2 || json[0] != '[' || json[^1] != ']')
            return false;
        string values = json[1..^1].Trim();
        if (values.Length == 0)
            return false;
        HashSet<int> unique = [];
        foreach (string value in values.Split(','))
        {
            if (!int.TryParse(value.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out int index)
                || index < 0 || index >= assets.Count)
            {
                assetIndexes.Clear();
                return false;
            }
            unique.Add(index);
        }
        assetIndexes.AddRange(unique.Order());
        return assetIndexes.Count > 0;
    }

    private void insertAssetSegments(int track, List<JsonObject> segments)
    {
        JsonArray tracks = lines();
        while (tracks.Count <= track)
            tracks.Add(new JsonObject { ["timeSegments"] = new JsonArray() });
        JsonObject trackData = (JsonObject)tracks[track]!;
        if (trackData["timeSegments"] is not JsonArray target)
        {
            target = new JsonArray();
            trackData["timeSegments"] = target;
        }
        foreach (JsonObject segment in segments)
            target.Add(segment);
        SetSegmentSelection(track, target.Count - 1);
        SelectedTimeTag = -1;
        SegmentSelected?.Invoke(SelectedTrack, SelectedSegment);
        SegmentChanged?.Invoke();
        Refresh();
    }

    private static JsonObject createFrame(double time) => new()
    {
        ["time"] = time,
        ["position"] = new JsonArray(0.0, 0.0),
        ["rotation"] = 0.0,
        ["scale"] = new JsonArray(1.0, 1.0),
    };

    private void updateCanvasSize()
    {
        double contentEnd = 5.0;
        int trackCount = lines().Count;
        foreach (JsonNode? lineNode in lines())
        {
            if (lineNode is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            foreach (JsonNode? segmentNode in segments)
                if (segmentNode is JsonObject segment)
                    contentEnd = Math.Max(contentEnd, AnimationEditor.number((segment["endFrame"] as JsonObject)?["time"]));
        }
        foreach (JsonNode? timeTagNode in timeTags())
        {
            if (timeTagNode is JsonObject timeTag)
                contentEnd = Math.Max(contentEnd, AnimationEditor.number(timeTag["time"]));
        }
        Width = (contentEnd + 1) * PixelsPerSecond;
        Height = HeaderHeight + (Math.Max(5, trackCount) + 1) * TrackHeight;
    }

    private JsonObject? segmentAt(int track, int segment)
    {
        return track >= 0 && track < lines().Count && lines()[track] is JsonObject line && line["timeSegments"] is JsonArray segments
            && segment >= 0 && segment < segments.Count ? segments[segment] as JsonObject : null;
    }

    private JsonArray lines()
    {
        JsonObject data = getData();
        if (data["timeLines"] is JsonArray lines)
            return lines;
        lines = new JsonArray();
        data["timeLines"] = lines;
        return lines;
    }

    private JsonArray timeTags()
    {
        JsonObject data = getData();
        if (data["timeTags"] is JsonArray tags)
            return tags;
        tags = new JsonArray();
        data["timeTags"] = tags;
        return tags;
    }

    private int sortTimeTags(JsonObject? selected)
    {
        JsonArray tags = timeTags();
        List<(JsonNode? Node, double Time, int Order)> ordered = tags
            .Select((node, index) => (
                Node: node,
                Time: node is JsonObject timeTag ? AnimationEditor.number(timeTag["time"]) : double.PositiveInfinity,
                Order: index
            ))
            .OrderBy(item => item.Time)
            .ThenBy(item => item.Order)
            .ToList();
        tags.Clear();
        int selectedIndex = -1;
        for (int index = 0; index < ordered.Count; index += 1)
        {
            JsonNode? node = ordered[index].Node;
            tags.Add(node);
            if (ReferenceEquals(node, selected))
                selectedIndex = index;
        }
        return selectedIndex;
    }

    private double timeTagDisplayX(int index)
    {
        if (GetTimeTag(index) is not JsonObject timeTag)
            return double.NegativeInfinity;
        double time = AnimationEditor.number(timeTag["time"]);
        int duplicate = 0;
        for (int previous = 0; previous < index; previous += 1)
        {
            if (GetTimeTag(previous) is JsonObject previousTag
                && Math.Abs(AnimationEditor.number(previousTag["time"]) - time) < 0.000001)
            {
                duplicate += 1;
            }
        }
        return time * PixelsPerSecond + duplicate * 14;
    }

    private int frameRate() => Math.Max(1, (int)AnimationEditor.number(getData()["frameRate"], 30));
    private double snap(double time) => Math.Round(time * frameRate()) / frameRate();
    private readonly record struct TimelineZoomAnchor(
        double Time,
        double ContentY,
        Point ViewportPoint);
}

public sealed class AnimationPreview : Control
{
    private readonly string projectPath;
    private readonly Func<JsonObject> getData;
    private readonly Dictionary<string, Bitmap> cache = new(StringComparer.OrdinalIgnoreCase);
    private Point dragStart;
    private double dragStartX;
    private double dragStartY;
    private bool dragging;

    public AnimationPreview(string projectPath, Func<JsonObject> getData)
    {
        this.projectPath = projectPath;
        this.getData = getData;
        Focusable = true;
    }

    public event Action<int, int>? SegmentSelected;
    public event Action? SegmentChanged;
    public double CurrentTime { get; set; }
    public int SelectedTrack { get; set; } = -1;
    public int SelectedSegment { get; set; } = -1;

    public void Refresh() => InvalidateVisual();

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#202020")), bounds);
        Point center = new(bounds.Width / 2, bounds.Height / 2);
        context.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#555555")), 1), new Point(center.X, 0), new Point(center.X, bounds.Height));
        context.DrawLine(new Pen(new SolidColorBrush(Color.Parse("#555555")), 1), new Point(0, center.Y), new Point(bounds.Width, center.Y));

        JsonArray assets = getData()["assets"] as JsonArray ?? [];
        JsonArray lines = getData()["timeLines"] as JsonArray ?? [];
        for (int track = 0; track < lines.Count; track += 1)
        {
            if (lines[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int index = 0; index < segments.Count; index += 1)
            {
                if (segments[index] is not JsonObject segment || segment["type"]?.GetValue<string>() == "sound")
                    continue;
                JsonObject? start = segment["startFrame"] as JsonObject;
                JsonObject? end = segment["endFrame"] as JsonObject;
                if (start is null || end is null)
                    continue;
                double startTime = AnimationEditor.number(start["time"]);
                double endTime = AnimationEditor.number(end["time"]);
                if (CurrentTime < startTime || CurrentTime > endTime)
                    continue;
                int assetIndex = (int)AnimationEditor.number(segment["asset"], -1);
                if (assetIndex < 0 || assetIndex >= assets.Count || assets[assetIndex] is not JsonValue assetValue || !assetValue.TryGetValue<string>(out string? assetName) || string.IsNullOrWhiteSpace(assetName))
                    continue;
                Bitmap? bitmap = getBitmap(assetName);
                if (bitmap is null)
                    continue;
                double factor = endTime - startTime < 0.0001 ? 0 : (CurrentTime - startTime) / (endTime - startTime);
                double x = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
                double y = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
                double scaleX = interpolate(start["scale"] as JsonArray, end["scale"] as JsonArray, 0, factor, 1);
                double scaleY = interpolate(start["scale"] as JsonArray, end["scale"] as JsonArray, 1, factor, 1);
                if (segment["flipX"]?.GetValue<bool>() == true)
                    scaleX *= -1;
                double rotation = interpolateValue(start["rotation"], end["rotation"], factor);
                double radians = rotation * Math.PI / 180.0;
                double cosine = Math.Cos(radians);
                double sine = Math.Sin(radians);
                Matrix transform = new(scaleX * cosine, scaleX * sine, -scaleY * sine, scaleY * cosine, center.X + x, center.Y + y);
                Rect localRect = new(-bitmap.Size.Width / 2, -bitmap.Size.Height / 2, bitmap.Size.Width, bitmap.Size.Height);
                using (context.PushTransform(transform))
                    context.DrawImage(bitmap, new Rect(bitmap.Size), localRect);
                if (track == SelectedTrack && index == SelectedSegment)
                    context.DrawRectangle(new Pen(Brushes.White, 2), new Rect(center.X + x - bitmap.Size.Width * Math.Abs(scaleX) / 2, center.Y + y - bitmap.Size.Height * Math.Abs(scaleY) / 2, bitmap.Size.Width * Math.Abs(scaleX), bitmap.Size.Height * Math.Abs(scaleY)));
            }
        }
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Point point = e.GetPosition(this);
        (int track, int segment) = hitTest(point);
        if (segment < 0)
        {
            SelectedTrack = -1;
            SelectedSegment = -1;
            SegmentSelected?.Invoke(-1, -1);
            return;
        }
        SelectedTrack = track;
        SelectedSegment = segment;
        SegmentSelected?.Invoke(track, segment);
        if (segmentAt(track, segment) is JsonObject selected)
        {
            JsonObject start = selected["startFrame"] as JsonObject ?? new JsonObject();
            JsonObject end = selected["endFrame"] as JsonObject ?? new JsonObject();
            double factor = timeFactor(start, end);
            dragStartX = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
            dragStartY = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
            dragStart = point;
            dragging = true;
            e.Pointer.Capture(this);
        }
        InvalidateVisual();
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (!dragging || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed || segmentAt(SelectedTrack, SelectedSegment) is not JsonObject segment)
            return;
        double x = snap(dragStartX + e.GetPosition(this).X - dragStart.X);
        double y = snap(dragStartY + e.GetPosition(this).Y - dragStart.Y);
        JsonObject start = segment["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject end = segment["endFrame"] as JsonObject ?? new JsonObject();
        double factor = timeFactor(start, end);
        double originalX = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
        double originalY = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
        shiftFrame(start, x - originalX, y - originalY);
        shiftFrame(end, x - originalX, y - originalY);
        segment["startFrame"] = start;
        segment["endFrame"] = end;
        InvalidateVisual();
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        if (dragging)
            SegmentChanged?.Invoke();
        dragging = false;
        e.Pointer.Capture(null);
    }

    private (int track, int segment) hitTest(Point point)
    {
        JsonArray lines = getData()["timeLines"] as JsonArray ?? [];
        for (int track = lines.Count - 1; track >= 0; track -= 1)
        {
            if (lines[track] is not JsonObject line || line["timeSegments"] is not JsonArray segments)
                continue;
            for (int index = segments.Count - 1; index >= 0; index -= 1)
            {
                if (segments[index] is not JsonObject segment || segment["type"]?.GetValue<string>() == "sound")
                    continue;
                JsonObject? start = segment["startFrame"] as JsonObject;
                JsonObject? end = segment["endFrame"] as JsonObject;
                if (start is null || end is null || CurrentTime < AnimationEditor.number(start["time"]) || CurrentTime > AnimationEditor.number(end["time"]))
                    continue;
                double factor = timeFactor(start, end);
                double x = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 0, factor);
                double y = interpolate(start["position"] as JsonArray, end["position"] as JsonArray, 1, factor);
                if (new Rect(Bounds.Width / 2 + x - 32, Bounds.Height / 2 + y - 32, 64, 64).Contains(point))
                    return (track, index);
            }
        }
        return (-1, -1);
    }

    private JsonObject? segmentAt(int track, int segment)
    {
        JsonArray lines = getData()["timeLines"] as JsonArray ?? [];
        return track >= 0 && track < lines.Count && lines[track] is JsonObject line && line["timeSegments"] is JsonArray segments
            && segment >= 0 && segment < segments.Count ? segments[segment] as JsonObject : null;
    }

    private Bitmap? getBitmap(string asset)
    {
        if (cache.TryGetValue(asset, out Bitmap? bitmap))
            return bitmap;
        string path = Path.Combine(projectPath, "Assets", "Animations", asset);
        if (!File.Exists(path))
            return null;
        bitmap = new Bitmap(path);
        cache[asset] = bitmap;
        return bitmap;
    }

    private static double interpolate(JsonArray? start, JsonArray? end, int index, double factor, double fallback = 0)
    {
        double a = AnimationEditor.number(start?.ElementAtOrDefault(index), fallback);
        double b = AnimationEditor.number(end?.ElementAtOrDefault(index), fallback);
        return a + (b - a) * factor;
    }

    private static double interpolateValue(JsonNode? start, JsonNode? end, double factor)
    {
        double a = AnimationEditor.number(start);
        return a + (AnimationEditor.number(end) - a) * factor;
    }

    private double timeFactor(JsonObject start, JsonObject end)
    {
        double startTime = AnimationEditor.number(start["time"]);
        double endTime = AnimationEditor.number(end["time"]);
        return endTime - startTime < 0.0001 ? 0 : (CurrentTime - startTime) / (endTime - startTime);
    }

    private static void shiftFrame(JsonObject frame, double x, double y)
    {
        JsonArray position = frame["position"] as JsonArray ?? new JsonArray(0.0, 0.0);
        position[0] = AnimationEditor.number(position.ElementAtOrDefault(0)) + x;
        position[1] = AnimationEditor.number(position.ElementAtOrDefault(1)) + y;
        frame["position"] = position;
    }

    private static double snap(double value) => Math.Round(value / 16) * 16;
}
