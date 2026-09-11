using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.VisualTree;
using Ludork.Services;
using Ludork.Views.Utils;
using System;
using System.Linq;
using System.Text.Json.Nodes;

namespace Ludork.Controls;

public sealed partial class ParticleEditor : UserControl
{
    private readonly GameDataService gameData;
    private readonly UiPreviewRuntimeService runtime;
    private readonly EditorDocument document;
    private readonly ListBox trackList = new();
    private readonly StackPanel properties = new() { Spacing = 8, Margin = new Thickness(8) };
    private readonly ContentControl curveHost = new();
    private readonly CheckBox solo = new() { Content = LocaleService.Get("PARTICLE_SOLO") };
    private JsonObject data;
    private int selectedTrack = -1;
    private bool refreshing;
    private bool committing;

    public ParticleEditor(GameDataService gameData, UiPreviewRuntimeService runtime, string key)
    {
        this.gameData = gameData;
        this.runtime = runtime;
        document = gameData.GetDocument("Particles", key) ?? throw new ArgumentException("Particle document was not found", nameof(key));
        data = document.Data!;
        trackList.SelectionChanged += (_, _) =>
        {
            if (refreshing)
                return;
            gameData.BreakHistoryGesture();
            selectedTrack = trackList.SelectedIndex;
            buildProperties();
            if (solo.IsChecked == true)
                requestLoad();
        };
        trackList.AddHandler(InputElement.ContextRequestedEvent, onTrackContextRequested, RoutingStrategies.Tunnel);
        solo.IsCheckedChanged += (_, _) => requestLoad();
        buildLayout();
        refreshTracks(0);
    }

    public string Key => document.Key;
    public void PausePreview() => setPlaying(false);
    private JsonArray tracks => data["tracks"] as JsonArray ?? new JsonArray();
    private JsonObject? track => selectedTrack >= 0 && selectedTrack < tracks.Count ? tracks[selectedTrack] as JsonObject : null;

    protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs args)
    {
        base.OnAttachedToVisualTree(args);
        document.Changed += onDocumentChanged;
        initializePlayback();
    }

    protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs args)
    {
        document.Changed -= onDocumentChanged;
        disposePlayback();
        base.OnDetachedFromVisualTree(args);
    }

    private void onDocumentChanged(object? sender, EventArgs args)
    {
        if (committing || document.Data is not JsonObject next)
            return;
        data = next;
        refreshTracks(selectedTrack);
        requestLoad();
    }

    private void buildLayout()
    {
        Grid root = new() { ColumnDefinitions = new ColumnDefinitions("170,5,*,5,Auto") };
        DockPanel trackPanel = new() { Margin = new Thickness(6) };
        DockPanel.SetDock(solo, Dock.Bottom);
        trackPanel.Children.Add(solo);
        trackPanel.Children.Add(trackList);
        root.Children.Add(trackPanel);
        Grid preview = new() { RowDefinitions = new RowDefinitions("*,Auto,260") };
        preview.Children.Add(createPreview());
        Control playback = createPlaybackToolbar();
        Grid.SetRow(playback, 1);
        preview.Children.Add(playback);
        Grid.SetRow(curveHost, 2);
        preview.Children.Add(curveHost);
        Grid.SetColumn(preview, 2);
        root.Children.Add(preview);
        ContentWidthScrollViewer inspector = new() { Content = properties, HorizontalScrollBarVisibility = ScrollBarVisibility.Auto };
        void updateInspectorWidth()
        {
            double available = root.Bounds.Width > 0
                ? Math.Max(0, root.Bounds.Width - root.ColumnDefinitions[0].ActualWidth - 10)
                : double.PositiveInfinity;
            root.ColumnDefinitions[4].MinWidth = Math.Min(inspector.RequiredWidth, available);
            root.ColumnDefinitions[4].MaxWidth = available;
            inspector.MaxWidth = available;
        }
        inspector.RequiredWidthChanged += (_, _) => updateInspectorWidth();
        root.LayoutUpdated += (_, _) => updateInspectorWidth();
        Grid.SetColumn(inspector, 4);
        root.Children.Add(inspector);
        foreach (int column in new[] { 1, 3 })
        {
            GridSplitter splitter = new() { Width = 5, HorizontalAlignment = HorizontalAlignment.Stretch, Background = Brushes.DimGray };
            Grid.SetColumn(splitter, column);
            root.Children.Add(splitter);
        }
        Content = root;
    }

    private void refreshTracks(int selection)
    {
        refreshing = true;
        trackList.ItemsSource = tracks.OfType<JsonObject>().Select(item =>
            (item["enabled"]?.GetValue<bool>() == false ? "○ " : "● ") + text(item, "name")).ToArray();
        selectedTrack = tracks.Count == 0 ? -1 : Math.Clamp(selection, 0, tracks.Count - 1);
        trackList.SelectedIndex = selectedTrack;
        refreshing = false;
        buildProperties();
    }

    private void onTrackContextRequested(object? sender, ContextRequestedEventArgs args)
    {
        bool requestedByPointer = args.TryGetPosition(trackList, out Point position);
        int index = requestedByPointer ? getTrackIndexAt(position) : selectedTrack;
        JsonObject? current = index >= 0 && index < tracks.Count ? tracks[index] as JsonObject : null;
        if (current is not null)
            trackList.SelectedIndex = index;
        MenuItem add = new() { Header = LocaleService.Get("PARTICLE_ADD_TRACK") };
        add.Click += (_, _) => addTrack();
        MenuItem duplicate = new() { Header = LocaleService.Get("PARTICLE_DUPLICATE_TRACK"), IsEnabled = current is not null };
        duplicate.Click += (_, _) =>
        {
            if (current is not null)
                duplicateTrack(current);
        };
        MenuItem remove = new() { Header = LocaleService.Get("PARTICLE_REMOVE_TRACK"), IsEnabled = current is not null };
        remove.Click += (_, _) =>
        {
            if (current is not null)
                removeTrack(current);
        };
        ContextMenu menu = new()
        {
            ItemsSource = new[] { add, duplicate, remove },
            Placement = requestedByPointer ? PlacementMode.Pointer : PlacementMode.Bottom,
        };
        trackList.ContextMenu = menu;
        Control target = !requestedByPointer && index >= 0 ? trackList.ContainerFromIndex(index) ?? trackList : trackList;
        menu.Open(target);
        args.Handled = true;
    }

    private int getTrackIndexAt(Point position)
    {
        Visual? visual = trackList.InputHitTest(position) as Visual;
        while (visual is not null && visual != trackList)
        {
            if (visual is ListBoxItem item)
                return trackList.IndexFromContainer(item);
            visual = visual.GetVisualParent();
        }
        return -1;
    }

    private void addTrack()
    {
        if (data["tracks"] is not JsonArray)
            data["tracks"] = new JsonArray();
        JsonObject created = ParticleAssetSchema.CreateTrack(uniqueTrackName(LocaleService.Get("PARTICLE_TRACK")));
        string glow = "/Game/Assets/Particles/Glow.png";
        if (GameAssetPath.TryResolveExistingFile(gameData.ProjectPath, glow, out _))
            created["texture"] = glow;
        tracks.Add(created);
        commit();
        refreshTracks(tracks.Count - 1);
    }

    private void duplicateTrack(JsonObject current)
    {
        int index = tracks.IndexOf(current);
        if (index < 0)
            return;
        JsonObject copy = (JsonObject)current.DeepClone();
        copy["name"] = uniqueTrackName(text(current, "name") + " (copy)");
        tracks.Insert(index + 1, copy);
        commit();
        refreshTracks(index + 1);
    }

    private void removeTrack(JsonObject current)
    {
        int index = tracks.IndexOf(current);
        if (index < 0)
            return;
        tracks.RemoveAt(index);
        commit();
        refreshTracks(index);
    }

    private string uniqueTrackName(string name)
    {
        string result = name;
        int index = 1;
        while (tracks.OfType<JsonObject>().Any(track => text(track, "name") == result))
            result = name + " " + index++;
        return result;
    }

    private void commit()
    {
        committing = true;
        gameData.UpdateParticle(Key, data);
        committing = false;
        requestLoad();
    }

    private static Button button(string locale, Action action)
    {
        Button result = new() { Content = LocaleService.Get(locale), HorizontalAlignment = HorizontalAlignment.Stretch };
        result.Click += (_, _) => action();
        return result;
    }

    private static string text(JsonObject value, string property, string fallback = "") => value[property]?.GetValue<string>() ?? fallback;
    private static double number(JsonNode? value, double fallback = 0) => ParticleAssetSchema.Number(value, fallback);
}
