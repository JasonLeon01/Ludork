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
    private readonly EditorDocument? resourceDocument;
    private readonly string initialKey;
    private string key => resourceDocument?.Key ?? initialKey;
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
        initialKey = key;
        resourceDocument = gameData.GetDocument("Animations", key);
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

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        if (resourceDocument is not null)
            resourceDocument.Changed += onDocumentChanged;
        onDocumentChanged(this, EventArgs.Empty);
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        if (resourceDocument is not null)
            resourceDocument.Changed -= onDocumentChanged;
        base.OnDetachedFromVisualTree(args);
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (resourceDocument?.Data is not JsonObject current || JsonNode.DeepEquals(current, data))
            return;
        data = current;
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
        nameBox.PropertyChanged += (_, args) =>
        {
            if (args.Property != TextBox.TextProperty || loadingInspector)
                return;
            string value = nameBox.Text ?? string.Empty;
            if (string.Equals(data["name"]?.GetValue<string>() ?? key, value, StringComparison.Ordinal))
                return;
            data["name"] = value;
            commit();
        };
        left.Children.Add(nameBox);

        left.Children.Add(new TextBlock { Text = LocaleService.Get("FRAME_RATE") });
        fpsBox.ItemsSource = new[] { "30", "60" };
        fpsBox.SelectionChanged += (_, _) =>
        {
            if (loadingInspector || fpsBox.SelectedItem is not string value || !int.TryParse(value, out int frameRate) || frameRate == this.frameRate())
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
        flipX.IsCheckedChanged += (_, _) =>
        {
            if (loadingInspector || getSelectedSegment() is not JsonObject segment
                || (segment["flipX"]?.GetValue<bool>() ?? false) == (flipX.IsChecked == true))
                return;
            segment["flipX"] = flipX.IsChecked == true;
            onSegmentChanged();
        };
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
            field.PropertyChanged += (_, args) =>
            {
                if (args.Property == TextBox.TextProperty)
                    updateInspector(field);
            };
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
        string[]? paths = await FileSelectorDialog.ShowMultipleAsync(owner, root,
            FileSelectorDialog.AllFilesFilter(star: true), LocaleService.Get(audio ? "ADD_AUDIO" : "ADD_ASSET"));
        if (paths is null)
            return;
        JsonArray assets = getAssets(true);
        foreach (string path in paths)
        {
            if (GameAssetPath.TryFromProjectFile(
                    gameData.ProjectPath,
                    path,
                    out string assetPath))
            {
                assets.Add(assetPath);
            }
        }
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
        JsonArray lines = getTimeLines(true);
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
        JsonObject start = segment!["startFrame"] as JsonObject ?? new JsonObject();
        JsonObject end = segment!["endFrame"] as JsonObject ?? new JsonObject();
        setFrameFields(start, startTime, startX, startY, startRotation, startScaleX, startScaleY);
        setFrameFields(end, endTime, endX, endY, endRotation, endScaleX, endScaleY);
        flipX.IsChecked = segment!["flipX"]?.GetValue<bool>() ?? false;
        loadingInspector = false;
    }

    private void updateInspector(TextBox editor)
    {
        if (loadingInspector || getSelectedSegment() is not JsonObject segment
            || !tryNumber(editor.Text, out double value))
            return;
        TextBox[] startEditors = [startTime, startX, startY, startRotation, startScaleX, startScaleY];
        TextBox[] endEditors = [endTime, endX, endY, endRotation, endScaleX, endScaleY];
        int index = Array.IndexOf(startEditors, editor);
        string frameName = "startFrame";
        if (index < 0)
        {
            index = Array.IndexOf(endEditors, editor);
            frameName = "endFrame";
        }
        if (index < 0)
            return;
        JsonObject frame = segment[frameName] is JsonObject current
            ? (JsonObject)current.DeepClone()
            : new JsonObject();
        if (index is 0 or 3)
        {
            string property = index == 0 ? "time" : "rotation";
            if (index == 0)
            {
                value = Math.Max(0, value);
                double first = frameName == "startFrame" ? value : number((segment["startFrame"] as JsonObject)?["time"]);
                double last = frameName == "endFrame" ? value : number((segment["endFrame"] as JsonObject)?["time"]);
                double minimumDuration = segment["type"]?.GetValue<string>() == "sound" ? 1.0 / frameRate() : 0.05;
                if (last < first + minimumDuration)
                    return;
            }
            if (number(frame[property]) == value)
                return;
            frame[property] = value;
        }
        else
        {
            string property = index < 3 ? "position" : "scale";
            int component = index < 3 ? index - 1 : index - 4;
            double fallback = property == "scale" ? 1 : 0;
            JsonArray values = frame[property] as JsonArray ?? new JsonArray();
            if (number(values.ElementAtOrDefault(component), fallback) == value)
                return;
            while (values.Count < 2)
                values.Add(fallback);
            values[component] = value;
            frame[property] = values;
        }
        segment[frameName] = frame;
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
                if (!GameAssetPath.TryResolveExistingFile(
                        gameData.ProjectPath,
                        assetName,
                        out string path))
                {
                    continue;
                }
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
            if (GameAssetPath.TryResolveExistingFile(
                    gameData.ProjectPath,
                    assetName,
                    out string path))
            {
                item.Child = new Image { Source = new Bitmap(path), Stretch = Stretch.Uniform };
            }
            else
                item.Child = createAssetText("Missing");
        }
        else if (isAudioAsset(assetName))
        {
            if (GameAssetPath.TryResolveExistingFile(
                    gameData.ProjectPath,
                    assetName,
                    out string path))
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

    private JsonArray getAssets(bool create = false)
    {
        if (data["assets"] is not JsonArray assets)
        {
            assets = new JsonArray();
            if (create)
                data["assets"] = assets;
        }
        return assets;
    }

    private JsonArray getTimeLines(bool create = false)
    {
        if (data["timeLines"] is not JsonArray lines)
        {
            lines = new JsonArray();
            if (create)
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

    private static bool tryNumber(string? value, out double result) => double.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out result) && double.IsFinite(result);
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
