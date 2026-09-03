using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using Ludork.Models;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed class UiAnimationTimelineEditor : UserControl
{
    private static readonly string[] TrackNames = ["translation", "rotation", "scale", "colour"];
    private readonly UiAssetEditorDocument document;
    private readonly GameDataService gameData;
    private readonly ListBox animationList = new()
    {
        MinWidth = 200,
        Background = Brushes.Transparent,
    };
    private readonly ComboBox targetBox = new() { MinWidth = 150 };
    private readonly TextBox nameBox = EditorInputs.CreateEditableTextBox();
    private readonly NumericUpDown durationBox = EditorInputs.CreateNumericUpDown(0.5m, 0.01m, 3600m, 0.01m, false);
    private readonly NumericUpDown pivotXBox = EditorInputs.CreateNumericUpDown(0.5m, 0m, 1m, 0.01m, false);
    private readonly NumericUpDown pivotYBox = EditorInputs.CreateNumericUpDown(0.5m, 0m, 1m, 0.01m, false);
    private readonly ComboBox trackBox = new() { MinWidth = 120 };
    private readonly NumericUpDown keyTimeBox = EditorInputs.CreateNumericUpDown(0m, 0m, 3600m, 0.01m, false);
    private readonly NumericUpDown keyXBox = EditorInputs.CreateNumericUpDown(0m, -1000000m, 1000000m, 0.01m, false);
    private readonly NumericUpDown keyYBox = EditorInputs.CreateNumericUpDown(0m, -1000000m, 1000000m, 0.01m, false);
    private readonly NumericUpDown keyZBox = EditorInputs.CreateNumericUpDown(0m, 0m, 255m, 1m, false);
    private readonly NumericUpDown keyWBox = EditorInputs.CreateNumericUpDown(0m, 0m, 255m, 1m, false);
    private readonly TextBlock keyXLabel = new() { VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock keyYLabel = new() { VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock keyZLabel = new() { VerticalAlignment = VerticalAlignment.Center };
    private readonly TextBlock keyWLabel = new() { VerticalAlignment = VerticalAlignment.Center };
    private readonly Button overrideAnimationButton = new();
    private readonly Button playButton = new();
    private readonly Button stopButton = new();
    private readonly Button addKeyButton = new();
    private readonly Button deleteKeyButton = new();
    private readonly Grid selectionEditor = new();
    private readonly UiAnimationTimelineSurface timeline;
    private readonly DispatcherTimer playbackTimer = new();
    private readonly Stopwatch playbackClock = new();
    private readonly List<AnimationChoice> choices = [];
    private readonly List<TargetChoice> targets = [];
    private JsonArray workingAnimations = [];
    private bool refreshing;
    private bool playing;
    private int selectedChoice = -1;
    private string selectedTrack = "translation";
    private int selectedKey = -1;
    private double currentTime;
    private double playbackOffset;

    public UiAnimationTimelineEditor(
        UiAssetEditorDocument document,
        GameDataService gameData)
    {
        this.document = document;
        this.gameData = gameData;
        timeline = new UiAnimationTimelineSurface();
        timeline.TimeChanged += (_, time) => setCurrentTime(time);
        timeline.SelectionChanged += (_, args) =>
        {
            selectedTrack = args.Track;
            selectedKey = args.KeyIndex;
            refreshKeyEditor();
            refreshTimeline();
        };
        timeline.KeyMoved += (_, args) => moveKey(args.Track, args.KeyIndex, args.Time);
        playbackTimer.Interval = TimeSpan.FromMilliseconds(1000.0 / 60.0);
        playbackTimer.Tick += (_, _) => advancePlayback();
        buildLayout();
        wireEvents();
        ApplyLocale();
        Refresh();
    }

    public event EventHandler? PreviewChanged;

    public bool IsCommitting { get; private set; }

    public UiPreviewAnimationSample? CurrentSample
    {
        get
        {
            AnimationChoice? choice = currentChoice();
            if (choice is null)
                return null;
            return new UiPreviewAnimationSample(choice.Name, choice.Target, currentTime);
        }
    }

    public void ApplyLocale()
    {
        overrideAnimationButton.Content = LocaleService.Get("UI_OVERRIDE_ANIMATION");
        playButton.Content = playing
            ? LocaleService.Get("UI_PAUSE_ANIMATION")
            : LocaleService.Get("PLAY_ANIMATION");
        stopButton.Content = LocaleService.Get("STOP_ANIMATION");
        addKeyButton.Content = LocaleService.Get("UI_ADD_KEY");
        deleteKeyButton.Content = LocaleService.Get("UI_DELETE_KEY");
        refreshTargetItems();
        refreshTrackItems();
    }

    public void Refresh()
    {
        string? identity = currentChoice()?.Identity;
        workingAnimations = document.Data["animations"] is JsonArray animations
            ? (JsonArray)animations.DeepClone()
            : [];
        rebuildChoices(identity);
    }

    public void StopPlayback()
    {
        playing = false;
        playbackTimer.Stop();
        playbackClock.Reset();
        playButton.Content = LocaleService.Get("PLAY_ANIMATION");
    }

    private void buildLayout()
    {
        Grid root = new()
        {
            ColumnDefinitions = new ColumnDefinitions("220,5,*"),
            Background = new SolidColorBrush(Color.Parse("#202020")),
        };
        Grid animationListPanel = new()
        {
            Margin = new Thickness(8),
        };
        animationListPanel.Children.Add(animationList);
        root.Children.Add(animationListPanel);

        GridSplitter splitter = new()
        {
            Width = 5,
            Background = new SolidColorBrush(Color.Parse("#383838")),
            ResizeDirection = GridResizeDirection.Columns,
        };
        Grid.SetColumn(splitter, 1);
        root.Children.Add(splitter);

        selectionEditor.RowDefinitions = new RowDefinitions("Auto,*,Auto");
        Grid.SetColumn(selectionEditor, 2);
        WrapPanel toolbar = new()
        {
            Orientation = Orientation.Horizontal,
            Margin = new Thickness(8, 5),
        };
        toolbar.Children.Add(overrideAnimationButton);
        toolbar.Children.Add(playButton);
        toolbar.Children.Add(stopButton);
        toolbar.Children.Add(field(LocaleService.Get("ANIMATION_NAME"), nameBox, 180));
        toolbar.Children.Add(field(LocaleService.Get("UI_ANIMATION_TARGET"), targetBox, 150));
        toolbar.Children.Add(field(LocaleService.Get("UI_ANIMATION_DURATION"), durationBox, 92));
        toolbar.Children.Add(field("Pivot X", pivotXBox, 82));
        toolbar.Children.Add(field("Pivot Y", pivotYBox, 82));
        selectionEditor.Children.Add(toolbar);

        ScrollViewer scroll = new()
        {
            HorizontalScrollBarVisibility = ScrollBarVisibility.Auto,
            VerticalScrollBarVisibility = ScrollBarVisibility.Disabled,
            Content = timeline,
        };
        Grid.SetRow(scroll, 1);
        selectionEditor.Children.Add(scroll);

        WrapPanel keyEditor = new()
        {
            Orientation = Orientation.Horizontal,
            Margin = new Thickness(8, 4, 8, 7),
        };
        Grid.SetRow(keyEditor, 2);
        keyEditor.Children.Add(trackBox);
        keyEditor.Children.Add(addKeyButton);
        keyEditor.Children.Add(deleteKeyButton);
        keyEditor.Children.Add(field(LocaleService.Get("UI_KEY_TIME"), keyTimeBox, 92));
        keyEditor.Children.Add(componentField(keyXLabel, keyXBox));
        StackPanel yField = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4,
            Children = { keyYLabel, keyYBox },
        };
        keyYBox.Width = 110;
        keyEditor.Children.Add(yField);
        StackPanel zField = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4,
            Children = { keyZLabel, keyZBox },
        };
        keyZBox.Width = 110;
        keyEditor.Children.Add(zField);
        StackPanel wField = new()
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4,
            Children = { keyWLabel, keyWBox },
        };
        keyWBox.Width = 110;
        keyEditor.Children.Add(wField);
        selectionEditor.Children.Add(keyEditor);
        root.Children.Add(selectionEditor);
        Content = root;
    }

    private static StackPanel componentField(TextBlock label, Control control)
    {
        control.Width = 110;
        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4,
            Children = { label, control },
        };
    }

    private static StackPanel field(string label, Control control, double width)
    {
        control.Width = width;
        return new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4,
            Children =
            {
                new TextBlock
                {
                    Text = label,
                    VerticalAlignment = VerticalAlignment.Center,
                },
                control,
            },
        };
    }

    private void wireEvents()
    {
        animationList.SelectionChanged += (_, _) =>
        {
            if (refreshing)
                return;
            selectedChoice = animationList.SelectedIndex;
            selectedKey = -1;
            currentTime = 0.0;
            StopPlayback();
            refreshSelectionEditor();
            notifyPreviewChanged();
        };
        animationList.PointerPressed += onAnimationListPointerPressed;
        animationList.AddHandler(
            InputElement.ContextRequestedEvent,
            onAnimationListContextRequested,
            RoutingStrategies.Tunnel);
        overrideAnimationButton.Click += (_, _) => overrideAnimation();
        playButton.Click += (_, _) => togglePlayback();
        stopButton.Click += (_, _) =>
        {
            StopPlayback();
            setCurrentTime(0.0);
        };
        nameBox.LostFocus += (_, _) => commitName();
        nameBox.KeyDown += (_, args) =>
        {
            if (args.Key == Key.Enter)
            {
                commitName();
                args.Handled = true;
            }
        };
        targetBox.SelectionChanged += (_, _) => changeTarget();
        durationBox.ValueChanged += (_, _) => changeDuration();
        pivotXBox.ValueChanged += (_, _) => changePivot();
        pivotYBox.ValueChanged += (_, _) => changePivot();
        trackBox.SelectionChanged += (_, _) =>
        {
            if (refreshing || trackBox.SelectedIndex < 0)
                return;
            selectedTrack = TrackNames[trackBox.SelectedIndex];
            selectedKey = -1;
            refreshKeyEditor();
            refreshTimeline();
        };
        addKeyButton.Click += (_, _) => addKey();
        deleteKeyButton.Click += (_, _) => deleteKey();
        keyTimeBox.ValueChanged += (_, _) => changeKeyTime();
        keyXBox.ValueChanged += (_, _) => changeKeyValue();
        keyYBox.ValueChanged += (_, _) => changeKeyValue();
        keyZBox.ValueChanged += (_, _) => changeKeyValue();
        keyWBox.ValueChanged += (_, _) => changeKeyValue();
    }

    private void onAnimationListPointerPressed(object? sender, PointerPressedEventArgs args)
    {
        PointerPoint point = args.GetCurrentPoint(animationList);
        ListBoxItem? item = getAnimationListItem(point.Position);
        if (point.Properties.IsRightButtonPressed)
        {
            animationList.SelectedIndex = item?.DataContext is string label
                ? choices.FindIndex(choice => string.Equals(choice.Label, label, StringComparison.Ordinal))
                : -1;
        }
        else if (point.Properties.IsLeftButtonPressed && item is null)
        {
            animationList.SelectedIndex = -1;
        }
    }

    private ListBoxItem? getAnimationListItem(Point position)
    {
        Visual? visual = animationList.InputHitTest(position) as Visual;
        if (visual is ListBoxItem item)
            return item;
        return visual?.GetVisualAncestors().OfType<ListBoxItem>().FirstOrDefault();
    }

    private void onAnimationListContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        showAnimationContextMenu();
        args.Handled = true;
    }

    private void showAnimationContextMenu()
    {
        MenuItem add = new() { Header = LocaleService.Get("UI_ADD_ANIMATION") };
        add.Click += (_, _) => addAnimation();
        MenuItem delete = new()
        {
            Header = LocaleService.Get("DELETE"),
            IsEnabled = currentChoice() is { Inherited: false },
        };
        delete.Click += (_, _) => deleteAnimation();
        ContextMenu menu = new()
        {
            ItemsSource = new object[] { add, delete },
        };
        animationList.ContextMenu = menu;
        menu.Open(animationList);
    }

    private void rebuildChoices(string? identity)
    {
        refreshing = true;
        choices.Clear();
        for (int index = 0; index < workingAnimations.Count; index++)
        {
            if (workingAnimations[index] is not JsonObject animation)
                continue;
            string name = stringValue(animation["name"]);
            string? target = nullableString(animation["target"]);
            choices.Add(new AnimationChoice(index, name, target, animation, false, string.Empty));
        }
        addInheritedChoices();
        animationList.ItemsSource = choices.Select(choice => choice.Label).ToArray();
        int nextSelection = identity is null
            ? -1
            : choices.FindIndex(choice => string.Equals(choice.Identity, identity, StringComparison.Ordinal));
        selectedChoice = nextSelection;
        animationList.SelectedIndex = nextSelection;
        refreshTargetItems();
        refreshTrackItems();
        refreshing = false;
        refreshSelectionEditor();
        notifyPreviewChanged();
    }

    private void addInheritedChoices()
    {
        foreach (JsonObject node in UiAssetSchema.EnumerateNodes(document.Data))
        {
            string controlId = stringValue(node["controlId"]);
            if (!UiAssetSchema.TryGetProjectAssetKey(controlId, out string assetKey))
                continue;
            string target = stringValue(node["name"]);
            string dataKey = UiAssetSchema.ToAssetDataKey(assetKey);
            if (!gameData.UiAssetsData.TryGetValue(dataKey, out JsonObject? asset)
                || asset["animations"] is not JsonArray animations)
            {
                continue;
            }
            foreach (JsonObject animation in animations.OfType<JsonObject>())
            {
                if (animation["target"] is not null)
                    continue;
                string name = stringValue(animation["name"]);
                bool overridden = choices.Any(choice => !choice.Inherited
                    && string.Equals(choice.Target, target, StringComparison.Ordinal)
                    && string.Equals(choice.Name, name, StringComparison.Ordinal));
                if (overridden)
                    continue;
                choices.Add(new AnimationChoice(
                    -1,
                    name,
                    target,
                    (JsonObject)animation.DeepClone(),
                    true,
                    assetKey));
            }
        }
    }

    private void refreshTargetItems()
    {
        string? selected = currentChoice()?.Target;
        targets.Clear();
        targets.Add(new TargetChoice(null, LocaleService.Get("UI_ANIMATION_GLOBAL")));
        foreach (JsonObject node in UiAssetSchema.EnumerateNodes(document.Data))
        {
            string name = stringValue(node["name"]);
            if (name.Length != 0)
                targets.Add(new TargetChoice(name, name));
        }
        targetBox.ItemsSource = targets.Select(target => target.Label).ToArray();
        targetBox.SelectedIndex = targets.FindIndex(target => string.Equals(
            target.Name,
            selected,
            StringComparison.Ordinal));
    }

    private void refreshTrackItems()
    {
        trackBox.ItemsSource = new[]
        {
            LocaleService.Get("TRANSLATION"),
            LocaleService.Get("ROTATION"),
            LocaleService.Get("SCALE"),
            LocaleService.Get("UI_ANIMATION_COLOUR"),
        };
        trackBox.SelectedIndex = Array.IndexOf(TrackNames, selectedTrack);
    }

    private void refreshSelectionEditor()
    {
        refreshing = true;
        AnimationChoice? choice = currentChoice();
        JsonObject? animation = currentAnimation();
        selectionEditor.IsVisible = choice is not null;
        bool editable = choice is { Inherited: false } && animation is not null;
        nameBox.IsEnabled = editable;
        targetBox.IsEnabled = editable;
        durationBox.IsEnabled = editable;
        pivotXBox.IsEnabled = editable;
        pivotYBox.IsEnabled = editable;
        overrideAnimationButton.IsVisible = choice is { Inherited: true };
        overrideAnimationButton.IsEnabled = choice is { Inherited: true };
        playButton.IsEnabled = animation is not null;
        stopButton.IsEnabled = animation is not null;
        nameBox.Text = choice?.Name ?? string.Empty;
        refreshTargetItems();
        durationBox.Value = decimalValue(animation?["duration"], 0.5m);
        JsonArray? pivot = animation?["pivot"] as JsonArray;
        pivotXBox.Value = decimalValue(pivot?[0], 0.5m);
        pivotYBox.Value = decimalValue(pivot?[1], 0.5m);
        currentTime = Math.Clamp(currentTime, 0.0, duration(animation));
        refreshing = false;
        refreshKeyEditor();
        refreshTimeline();
    }

    private void refreshKeyEditor()
    {
        refreshing = true;
        JsonObject? animation = currentAnimation();
        AnimationChoice? choice = currentChoice();
        JsonObject? key = keyAt(animation, selectedTrack, selectedKey);
        bool editable = choice is { Inherited: false } && animation is not null;
        bool hasKey = editable && key is not null;
        addKeyButton.IsEnabled = editable;
        deleteKeyButton.IsEnabled = hasKey;
        keyTimeBox.IsEnabled = hasKey;
        keyXBox.IsEnabled = hasKey;
        bool vector = selectedTrack is "translation" or "scale";
        bool colour = selectedTrack == "colour";
        keyXLabel.Text = colour ? "R" : vector ? "X" : LocaleService.Get("UI_KEY_VALUE");
        keyYLabel.Text = colour ? "G" : "Y";
        keyZLabel.Text = "B";
        keyWLabel.Text = "A";
        keyYBox.IsVisible = vector || colour;
        keyYLabel.IsVisible = vector || colour;
        keyZBox.IsVisible = colour;
        keyZLabel.IsVisible = colour;
        keyWBox.IsVisible = colour;
        keyWLabel.IsVisible = colour;
        keyYBox.IsEnabled = hasKey && (vector || colour);
        keyZBox.IsEnabled = hasKey && colour;
        keyWBox.IsEnabled = hasKey && colour;
        keyXBox.Increment = colour ? 1m : 0.01m;
        keyYBox.Increment = colour ? 1m : 0.01m;
        keyTimeBox.Maximum = decimalValue(animation?["duration"], 0.5m);
        keyTimeBox.Value = decimalValue(key?["time"], 0m);
        if (key?["value"] is JsonArray value)
        {
            keyXBox.Value = decimalValue(value[0], 0m);
            keyYBox.Value = decimalValue(value[1], 0m);
            keyZBox.Value = decimalValue(value.Count > 2 ? value[2] : null, 0m);
            keyWBox.Value = decimalValue(value.Count > 3 ? value[3] : null, 0m);
        }
        else
        {
            keyXBox.Value = decimalValue(key?["value"], identityValue(selectedTrack));
            keyYBox.Value = 0m;
            keyZBox.Value = 0m;
            keyWBox.Value = 0m;
        }
        if (colour)
        {
            keyXBox.Minimum = 0m;
            keyXBox.Maximum = 255m;
            keyYBox.Minimum = 0m;
            keyYBox.Maximum = 255m;
        }
        else if (selectedTrack == "scale")
        {
            keyXBox.Minimum = 0m;
            keyXBox.Maximum = 1000000m;
            keyYBox.Minimum = 0m;
            keyYBox.Maximum = 1000000m;
        }
        else
        {
            keyXBox.Minimum = -1000000m;
            keyXBox.Maximum = 1000000m;
            keyYBox.Minimum = -1000000m;
            keyYBox.Maximum = 1000000m;
        }
        refreshing = false;
    }

    private void refreshTimeline()
    {
        timeline.SetState(
            currentAnimation(),
            selectedTrack,
            selectedKey,
            currentTime,
            currentChoice() is { Inherited: false });
    }

    private void addAnimation()
    {
        string? target = targets.ElementAtOrDefault(targetBox.SelectedIndex)?.Name;
        string name = uniqueName("Animation", target, -1);
        JsonObject animation = new()
        {
            ["name"] = name,
            ["target"] = target,
            ["duration"] = 0.5,
            ["pivot"] = new JsonArray(0.5, 0.5),
            ["tracks"] = new JsonObject(),
        };
        workingAnimations.Add(animation);
        commitAnimations();
        rebuildChoices(targetIdentity(name, target, false));
    }

    private void deleteAnimation()
    {
        AnimationChoice? choice = currentChoice();
        if (choice is not { Inherited: false } || choice.Index < 0)
            return;
        workingAnimations.RemoveAt(choice.Index);
        selectedChoice = -1;
        StopPlayback();
        commitAnimations();
        rebuildChoices(null);
    }

    private void overrideAnimation()
    {
        AnimationChoice? choice = currentChoice();
        if (choice is not { Inherited: true })
            return;
        JsonObject animation = (JsonObject)choice.Data.DeepClone();
        animation["target"] = choice.Target;
        workingAnimations.Add(animation);
        commitAnimations();
        rebuildChoices(targetIdentity(choice.Name, choice.Target, false));
    }

    private void commitName()
    {
        AnimationChoice? choice = currentChoice();
        JsonObject? animation = currentAnimation();
        if (refreshing || choice is not { Inherited: false } || animation is null)
            return;
        string requested = nameBox.Text?.Trim() ?? string.Empty;
        string name = uniqueName(requested.Length == 0 ? "Animation" : requested, choice.Target, choice.Index);
        if (string.Equals(choice.Name, name, StringComparison.Ordinal))
            return;
        animation["name"] = name;
        commitAnimations();
        rebuildChoices(targetIdentity(name, choice.Target, false));
    }

    private void changeTarget()
    {
        AnimationChoice? choice = currentChoice();
        JsonObject? animation = currentAnimation();
        if (refreshing || choice is not { Inherited: false } || animation is null
            || targetBox.SelectedIndex < 0 || targetBox.SelectedIndex >= targets.Count)
        {
            return;
        }
        string? target = targets[targetBox.SelectedIndex].Name;
        string name = uniqueName(choice.Name, target, choice.Index);
        animation["target"] = target;
        animation["name"] = name;
        commitAnimations();
        rebuildChoices(targetIdentity(name, target, false));
    }

    private void changeDuration()
    {
        JsonObject? animation = editableAnimation();
        if (refreshing || animation is null)
            return;
        double next = decimal.ToDouble(durationBox.Value ?? 0.5m);
        double maximumKey = maximumKeyTime(animation);
        if (next < maximumKey)
        {
            refreshing = true;
            durationBox.Value = (decimal)maximumKey;
            refreshing = false;
            return;
        }
        animation["duration"] = next;
        currentTime = Math.Min(currentTime, next);
        commitAnimations();
        refreshKeyEditor();
        refreshTimeline();
    }

    private void changePivot()
    {
        JsonObject? animation = editableAnimation();
        if (refreshing || animation is null)
            return;
        animation["pivot"] = new JsonArray(
            decimal.ToDouble(pivotXBox.Value ?? 0.5m),
            decimal.ToDouble(pivotYBox.Value ?? 0.5m));
        commitAnimations();
    }

    private void addKey()
    {
        JsonObject? animation = editableAnimation();
        if (animation is null)
            return;
        JsonObject tracks = animation["tracks"] as JsonObject ?? new JsonObject();
        animation["tracks"] = tracks;
        JsonArray keys = tracks[selectedTrack] as JsonArray ?? new JsonArray();
        tracks[selectedTrack] = keys;
        for (int index = 0; index < keys.Count; index++)
        {
            if (keys[index] is JsonObject key
                && Math.Abs(number(key["time"]) - currentTime) < 0.0001)
            {
                selectedKey = index;
                refreshKeyEditor();
                refreshTimeline();
                return;
            }
        }
        JsonNode value = selectedTrack switch
        {
            "translation" or "scale" => new JsonArray(
                (double)identityValue(selectedTrack),
                (double)identityValue(selectedTrack)),
            "colour" => new JsonArray(255, 255, 255, 255),
            _ => JsonValue.Create((double)identityValue(selectedTrack)),
        };
        JsonObject newKey = new()
        {
            ["time"] = currentTime,
            ["value"] = value,
        };
        int insertion = 0;
        while (insertion < keys.Count
               && keys[insertion] is JsonObject existing
               && number(existing["time"]) < currentTime)
        {
            insertion++;
        }
        keys.Insert(insertion, newKey);
        selectedKey = insertion;
        commitAnimations();
        refreshKeyEditor();
        refreshTimeline();
    }

    private void deleteKey()
    {
        JsonObject? animation = editableAnimation();
        JsonObject? tracks = animation?["tracks"] as JsonObject;
        JsonArray? keys = tracks?[selectedTrack] as JsonArray;
        if (keys is null || selectedKey < 0 || selectedKey >= keys.Count)
            return;
        keys.RemoveAt(selectedKey);
        if (keys.Count == 0)
            tracks!.Remove(selectedTrack);
        selectedKey = -1;
        commitAnimations();
        refreshKeyEditor();
        refreshTimeline();
    }

    private void changeKeyTime()
    {
        if (refreshing)
            return;
        moveKey(selectedTrack, selectedKey, decimal.ToDouble(keyTimeBox.Value ?? 0m));
    }

    private void moveKey(string track, int keyIndex, double time)
    {
        JsonObject? animation = editableAnimation();
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        JsonObject? key = keyAt(animation, track, keyIndex);
        if (keys is null || key is null)
            return;
        double minimum = keyIndex == 0
            ? 0.0
            : number((keys[keyIndex - 1] as JsonObject)?["time"]) + 0.001;
        double maximum = keyIndex + 1 >= keys.Count
            ? duration(animation)
            : number((keys[keyIndex + 1] as JsonObject)?["time"]) - 0.001;
        double next = Math.Clamp(Math.Round(time, 3), minimum, maximum);
        if (Math.Abs(number(key["time"]) - next) < 0.000001)
            return;
        key["time"] = next;
        currentTime = next;
        commitAnimations();
        refreshKeyEditor();
        refreshTimeline();
        notifyPreviewChanged();
    }

    private void changeKeyValue()
    {
        JsonObject? animation = editableAnimation();
        JsonObject? key = keyAt(animation, selectedTrack, selectedKey);
        if (refreshing || key is null)
            return;
        double x = decimal.ToDouble(keyXBox.Value ?? identityValue(selectedTrack));
        if (selectedTrack is "translation" or "scale")
        {
            double y = decimal.ToDouble(keyYBox.Value ?? identityValue(selectedTrack));
            key["value"] = new JsonArray(x, y);
        }
        else if (selectedTrack == "colour")
        {
            key["value"] = new JsonArray(
                decimal.ToInt32(keyXBox.Value ?? 255m),
                decimal.ToInt32(keyYBox.Value ?? 255m),
                decimal.ToInt32(keyZBox.Value ?? 255m),
                decimal.ToInt32(keyWBox.Value ?? 255m));
        }
        else
        {
            key["value"] = x;
        }
        commitAnimations();
        if (selectedTrack == "colour")
            refreshKeyEditor();
        refreshTimeline();
        notifyPreviewChanged();
    }

    private void togglePlayback()
    {
        if (currentAnimation() is null)
            return;
        if (playing)
        {
            StopPlayback();
            return;
        }
        if (currentTime >= duration(currentAnimation()))
            currentTime = 0.0;
        playbackOffset = currentTime;
        playbackClock.Restart();
        playing = true;
        playButton.Content = LocaleService.Get("UI_PAUSE_ANIMATION");
        playbackTimer.Start();
    }

    private void advancePlayback()
    {
        JsonObject? animation = currentAnimation();
        if (!playing || animation is null)
        {
            StopPlayback();
            return;
        }
        double end = duration(animation);
        double next = playbackOffset + playbackClock.Elapsed.TotalSeconds;
        if (next >= end)
        {
            next = end;
            StopPlayback();
        }
        setCurrentTime(next);
    }

    private void setCurrentTime(double value)
    {
        currentTime = Math.Clamp(value, 0.0, duration(currentAnimation()));
        refreshTimeline();
        notifyPreviewChanged();
    }

    private void commitAnimations()
    {
        IsCommitting = true;
        try
        {
            document.SetAnimations((JsonArray)workingAnimations.DeepClone());
        }
        finally
        {
            IsCommitting = false;
        }
    }

    private void notifyPreviewChanged()
    {
        PreviewChanged?.Invoke(this, EventArgs.Empty);
    }

    private AnimationChoice? currentChoice()
    {
        return selectedChoice >= 0 && selectedChoice < choices.Count
            ? choices[selectedChoice]
            : null;
    }

    private JsonObject? currentAnimation()
    {
        return currentChoice()?.Data;
    }

    private JsonObject? editableAnimation()
    {
        return currentChoice() is { Inherited: false } ? currentAnimation() : null;
    }

    private string uniqueName(string requested, string? target, int excludedIndex)
    {
        string baseName = requested.Trim();
        if (baseName.Length == 0)
            baseName = "Animation";
        string result = baseName;
        int suffix = 2;
        JsonObject? excluded = excludedIndex >= 0 && excludedIndex < workingAnimations.Count
            ? workingAnimations[excludedIndex] as JsonObject
            : null;
        while (workingAnimations.OfType<JsonObject>().Any(animation =>
                   !ReferenceEquals(animation, excluded)
                   && string.Equals(nullableString(animation["target"]), target, StringComparison.Ordinal)
                   && string.Equals(stringValue(animation["name"]), result, StringComparison.Ordinal)))
        {
            result = baseName + suffix;
            suffix++;
        }
        return result;
    }

    private static JsonObject? keyAt(JsonObject? animation, string track, int index)
    {
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        return keys is not null && index >= 0 && index < keys.Count
            ? keys[index] as JsonObject
            : null;
    }

    private static double maximumKeyTime(JsonObject animation)
    {
        double result = 0.0;
        if (animation["tracks"] is not JsonObject tracks)
            return result;
        foreach (JsonArray keys in tracks.Select(pair => pair.Value).OfType<JsonArray>())
        {
            foreach (JsonObject key in keys.OfType<JsonObject>())
                result = Math.Max(result, number(key["time"]));
        }
        return result;
    }

    private static double duration(JsonObject? animation)
    {
        return animation is null ? 0.0 : Math.Max(0.01, number(animation["duration"], 0.5));
    }

    private static decimal identityValue(string track)
    {
        return track == "scale" ? 1m : 0m;
    }

    private static string targetIdentity(string name, string? target, bool inherited)
    {
        return (inherited ? "I" : "L") + "\u001f" + (target ?? string.Empty) + "\u001f" + name;
    }

    private static string stringValue(JsonNode? value)
    {
        return value is JsonValue json && json.TryGetValue(out string? result)
            ? result ?? string.Empty
            : string.Empty;
    }

    private static string? nullableString(JsonNode? value)
    {
        string result = stringValue(value);
        return result.Length == 0 ? null : result;
    }

    private static decimal decimalValue(JsonNode? value, decimal fallback)
    {
        return (decimal)number(value, decimal.ToDouble(fallback));
    }

    private static double number(JsonNode? value, double fallback = 0.0)
    {
        if (value is not JsonValue json)
            return fallback;
        if (json.TryGetValue(out double doubleValue))
            return doubleValue;
        if (json.TryGetValue(out long integerValue))
            return integerValue;
        if (json.TryGetValue(out decimal decimalValue))
            return decimal.ToDouble(decimalValue);
        return fallback;
    }

    private sealed record AnimationChoice(
        int Index,
        string Name,
        string? Target,
        JsonObject Data,
        bool Inherited,
        string Source)
    {
        public string Identity => targetIdentity(Name, Target, Inherited);
        public string Label => Target is null
            ? Name + " · Global"
            : Name + " · " + Target + (Inherited ? " · " + Source : string.Empty);
    }

    private sealed record TargetChoice(string? Name, string Label);
}

public sealed class UiAnimationKeySelectionEventArgs : EventArgs
{
    public UiAnimationKeySelectionEventArgs(string track, int keyIndex)
    {
        Track = track;
        KeyIndex = keyIndex;
    }

    public string Track { get; }
    public int KeyIndex { get; }
}

public sealed class UiAnimationKeyMoveEventArgs : EventArgs
{
    public UiAnimationKeyMoveEventArgs(string track, int keyIndex, double time)
    {
        Track = track;
        KeyIndex = keyIndex;
        Time = time;
    }

    public string Track { get; }
    public int KeyIndex { get; }
    public double Time { get; }
}

public sealed class UiAnimationTimelineSurface : Control
{
    private const double HeaderHeight = 26;
    private const double TrackHeight = 28;
    private const double LabelWidth = 104;
    private const double PixelsPerSecond = 240;
    private static readonly string[] TrackNames = ["translation", "rotation", "scale", "colour"];
    private JsonObject? animation;
    private string selectedTrack = "translation";
    private int selectedKey = -1;
    private double currentTime;
    private bool editable;
    private bool draggingKey;

    public UiAnimationTimelineSurface()
    {
        Height = HeaderHeight + TrackNames.Length * TrackHeight;
        MinWidth = 720;
        Focusable = true;
    }

    public event EventHandler<double>? TimeChanged;
    public event EventHandler<UiAnimationKeySelectionEventArgs>? SelectionChanged;
    public event EventHandler<UiAnimationKeyMoveEventArgs>? KeyMoved;

    public void SetState(
        JsonObject? animation,
        string selectedTrack,
        int selectedKey,
        double currentTime,
        bool editable)
    {
        this.animation = animation;
        this.selectedTrack = selectedTrack;
        this.selectedKey = selectedKey;
        this.currentTime = currentTime;
        this.editable = editable;
        Width = Math.Max(720, LabelWidth + duration() * PixelsPerSecond + 40);
        InvalidateVisual();
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        Rect bounds = new(Bounds.Size);
        context.FillRectangle(new SolidColorBrush(Color.Parse("#262626")), bounds);
        context.FillRectangle(
            new SolidColorBrush(Color.Parse("#383838")),
            new Rect(LabelWidth, 0, Math.Max(0, bounds.Width - LabelWidth), HeaderHeight));
        context.FillRectangle(
            new SolidColorBrush(Color.Parse("#303030")),
            new Rect(0, 0, LabelWidth, bounds.Height));
        drawRuler(context, bounds);
        for (int trackIndex = 0; trackIndex < TrackNames.Length; trackIndex++)
            drawTrack(context, bounds, trackIndex);
        double playheadX = LabelWidth + currentTime * PixelsPerSecond;
        Pen playheadPen = new(new SolidColorBrush(Color.Parse("#ff5c5c")), 1);
        context.DrawLine(playheadPen, new Point(playheadX, 0), new Point(playheadX, bounds.Height));
        StreamGeometry head = new();
        using (StreamGeometryContext geometry = head.Open())
        {
            geometry.BeginFigure(new Point(playheadX, 16), true);
            geometry.LineTo(new Point(playheadX - 5, 10));
            geometry.LineTo(new Point(playheadX - 5, 0));
            geometry.LineTo(new Point(playheadX + 5, 0));
            geometry.LineTo(new Point(playheadX + 5, 10));
            geometry.EndFigure(true);
        }
        context.DrawGeometry(new SolidColorBrush(Color.Parse("#ff5c5c")), null, head);
    }

    protected override void OnPointerPressed(PointerPressedEventArgs e)
    {
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        Focus();
        Point position = e.GetPosition(this);
        if (position.X < LabelWidth || position.Y < HeaderHeight)
        {
            if (position.X >= LabelWidth)
                setTime(position.X);
            return;
        }
        int trackIndex = Math.Clamp(
            (int)((position.Y - HeaderHeight) / TrackHeight),
            0,
            TrackNames.Length - 1);
        string track = TrackNames[trackIndex];
        int keyIndex = hitKey(track, position.X);
        selectedTrack = track;
        selectedKey = keyIndex;
        SelectionChanged?.Invoke(this, new UiAnimationKeySelectionEventArgs(track, keyIndex));
        if (keyIndex >= 0)
        {
            JsonObject? key = keyAt(track, keyIndex);
            if (key is not null)
                TimeChanged?.Invoke(this, number(key["time"]));
            if (editable)
            {
                draggingKey = true;
                e.Pointer.Capture(this);
            }
        }
        else
        {
            setTime(position.X);
        }
        e.Handled = true;
    }

    protected override void OnPointerMoved(PointerEventArgs e)
    {
        if (!draggingKey || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;
        double time = timeAt(e.GetPosition(this).X);
        KeyMoved?.Invoke(this, new UiAnimationKeyMoveEventArgs(selectedTrack, selectedKey, time));
        e.Handled = true;
    }

    protected override void OnPointerReleased(PointerReleasedEventArgs e)
    {
        if (!draggingKey)
            return;
        draggingKey = false;
        e.Pointer.Capture(null);
        e.Handled = true;
    }

    protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
    {
        draggingKey = false;
        base.OnPointerCaptureLost(e);
    }

    private void drawRuler(DrawingContext context, Rect bounds)
    {
        double end = Math.Max(duration(), (bounds.Width - LabelWidth) / PixelsPerSecond);
        for (double time = 0.0; time <= end + 0.0001; time += 0.25)
        {
            double x = LabelWidth + time * PixelsPerSecond;
            bool whole = Math.Abs(time - Math.Round(time)) < 0.0001;
            Pen pen = new(
                new SolidColorBrush(Color.Parse(whole ? "#606060" : "#454545")),
                1);
            context.DrawLine(pen, new Point(x, whole ? 0 : 14), new Point(x, bounds.Height));
            if (!whole)
                continue;
            FormattedText label = new(
                time.ToString("0", CultureInfo.InvariantCulture) + "s",
                CultureInfo.CurrentUICulture,
                FlowDirection.LeftToRight,
                Typeface.Default,
                10,
                new SolidColorBrush(Color.Parse("#bbbbbb")));
            context.DrawText(label, new Point(x + 3, 2));
        }
    }

    private void drawTrack(DrawingContext context, Rect bounds, int trackIndex)
    {
        string track = TrackNames[trackIndex];
        double y = HeaderHeight + trackIndex * TrackHeight;
        string fill = string.Equals(track, selectedTrack, StringComparison.Ordinal)
            ? "#354553"
            : trackIndex % 2 == 0 ? "#2d2d2d" : "#292929";
        context.FillRectangle(
            new SolidColorBrush(Color.Parse(fill)),
            new Rect(0, y, bounds.Width, TrackHeight));
        FormattedText label = new(
            track,
            CultureInfo.CurrentUICulture,
            FlowDirection.LeftToRight,
            Typeface.Default,
            11,
            Brushes.White);
        context.DrawText(label, new Point(8, y + 6));
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        if (keys is null)
            return;
        for (int keyIndex = 0; keyIndex < keys.Count; keyIndex++)
        {
            if (keys[keyIndex] is not JsonObject key)
                continue;
            double x = LabelWidth + number(key["time"]) * PixelsPerSecond;
            double centerY = y + TrackHeight / 2.0;
            StreamGeometry diamond = new();
            using (StreamGeometryContext geometry = diamond.Open())
            {
                geometry.BeginFigure(new Point(x, centerY - 6), true);
                geometry.LineTo(new Point(x + 6, centerY));
                geometry.LineTo(new Point(x, centerY + 6));
                geometry.LineTo(new Point(x - 6, centerY));
                geometry.EndFigure(true);
            }
            bool selected = string.Equals(track, selectedTrack, StringComparison.Ordinal)
                && keyIndex == selectedKey;
            context.DrawGeometry(
                new SolidColorBrush(Color.Parse(selected ? "#ffd166" : "#8ab4f8")),
                selected ? new Pen(Brushes.White, 1) : null,
                diamond);
        }
    }

    private int hitKey(string track, double x)
    {
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        if (keys is null)
            return -1;
        int result = -1;
        double distance = 8.0;
        for (int index = 0; index < keys.Count; index++)
        {
            if (keys[index] is not JsonObject key)
                continue;
            double current = Math.Abs(LabelWidth + number(key["time"]) * PixelsPerSecond - x);
            if (current <= distance)
            {
                result = index;
                distance = current;
            }
        }
        return result;
    }

    private JsonObject? keyAt(string track, int index)
    {
        JsonArray? keys = (animation?["tracks"] as JsonObject)?[track] as JsonArray;
        return keys is not null && index >= 0 && index < keys.Count
            ? keys[index] as JsonObject
            : null;
    }

    private void setTime(double x)
    {
        TimeChanged?.Invoke(this, timeAt(x));
    }

    private double timeAt(double x)
    {
        return Math.Clamp((x - LabelWidth) / PixelsPerSecond, 0.0, duration());
    }

    private double duration()
    {
        return Math.Max(0.01, number(animation?["duration"], 0.5));
    }

    private static double number(JsonNode? value, double fallback = 0.0)
    {
        if (value is not JsonValue json)
            return fallback;
        if (json.TryGetValue(out double doubleValue))
            return doubleValue;
        if (json.TryGetValue(out long integerValue))
            return integerValue;
        if (json.TryGetValue(out decimal decimalValue))
            return decimal.ToDouble(decimalValue);
        return fallback;
    }
}
